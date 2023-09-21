#include "kvm/kvm.h"
#include "kvm/term.h"
#include "kvm/util.h"
#include "kvm/8250-serial.h"
#include "kvm/virtio-console.h"
#include "kvm/fdt.h"

#include "arm-common/gic.h"
#include "asm/realm.h"

#include <sys/resource.h>

#include <linux/kernel.h>
#include <linux/kvm.h>
#include <linux/sizes.h>

struct kvm_ext kvm_req_ext[] = {
	{ DEFINE_KVM_EXT(KVM_CAP_IRQCHIP) },
	{ DEFINE_KVM_EXT(KVM_CAP_ONE_REG) },
	{ DEFINE_KVM_EXT(KVM_CAP_ARM_PSCI) },
	{ 0, 0 },
};

bool kvm__arch_cpu_supports_vm(void)
{
	/* The KVM capability check is enough. */
	return true;
}

static void try_increase_mlock_limit(struct kvm *kvm)
{
	u64 size = kvm->arch.ram_alloc_size;
	struct rlimit mlock_limit, new_limit;

	if (getrlimit(RLIMIT_MEMLOCK, &mlock_limit)) {
		perror("getrlimit(RLIMIT_MEMLOCK)");
		return;
	}

	if (mlock_limit.rlim_cur > size)
		return;

	new_limit.rlim_cur = size;
	new_limit.rlim_max = max((rlim_t)size, mlock_limit.rlim_max);
	/* Requires CAP_SYS_RESOURCE capability. */
	setrlimit(RLIMIT_MEMLOCK, &new_limit);
}

void kvm__init_ram(struct kvm *kvm)
{
	u64 phys_start, phys_size;
	void *host_mem;
	int err;

	/*
	 * Allocate guest memory. We must align our buffer to 64K to
	 * correlate with the maximum guest page size for virtio-mmio.
	 * If using THP, then our minimal alignment becomes 2M.
	 * 2M trumps 64K, so let's go with that.
	 */
	kvm->ram_size = kvm->cfg.ram_size;
	kvm->arch.ram_alloc_size = kvm->ram_size;
	if (!kvm->cfg.hugetlbfs_path)
		kvm->arch.ram_alloc_size += SZ_2M;
	kvm->arch.ram_alloc_start = mmap_anon_or_hugetlbfs(kvm,
						kvm->cfg.hugetlbfs_path,
						kvm->arch.ram_alloc_size);

	if (kvm->arch.ram_alloc_start == MAP_FAILED)
		die("Failed to map %lld bytes for guest memory (%d)",
		    kvm->arch.ram_alloc_size, errno);

	kvm->ram_start = (void *)ALIGN((unsigned long)kvm->arch.ram_alloc_start,
					SZ_2M);

	/*
	 * Do not merge pages if this is a Realm.
	 *  a) We cannot replace a page in realm stage2 without export/import
	 *
	 * Pin the realm memory until we have export/import, due to the same
	 * reason as above.
	 *
	 * Use mlock2(,,MLOCK_ONFAULT) to allow faulting in pages and thus
	 * allowing to lazily populate the PAR.
	 */
	if (kvm__is_realm(kvm)) {
		int ret;

		try_increase_mlock_limit(kvm);
		ret = mlock2(kvm->arch.ram_alloc_start, kvm->arch.ram_alloc_size,
			     MLOCK_ONFAULT);
		if (ret)
			die_perror("mlock2");
	} else {
		madvise(kvm->arch.ram_alloc_start, kvm->arch.ram_alloc_size, MADV_MERGEABLE);
	}

	madvise(kvm->arch.ram_alloc_start, kvm->arch.ram_alloc_size,
		MADV_HUGEPAGE);

	phys_start	= kvm->cfg.ram_addr;
	phys_size	= kvm->ram_size;
	host_mem	= kvm->ram_start;

	err = kvm__register_ram(kvm, phys_start, phys_size, host_mem);
	if (err)
		die("Failed to register %lld bytes of memory at physical "
		    "address 0x%llx [err %d]", phys_size, phys_start, err);

	kvm->arch.memory_guest_start = phys_start;

	pr_debug("RAM created at 0x%llx - 0x%llx",
		 phys_start, phys_start + phys_size - 1);
}

void kvm__arch_delete_ram(struct kvm *kvm)
{
	munmap(kvm->arch.ram_alloc_start, kvm->arch.ram_alloc_size);
}

void kvm__arch_read_term(struct kvm *kvm)
{
	serial8250__update_consoles(kvm);
	virtio_console__inject_interrupt(kvm);
}

void kvm__arch_set_cmdline(char *cmdline, bool video)
{
}

void kvm__arch_init(struct kvm *kvm)
{
	if (kvm__is_realm(kvm))
		kvm_arm_realm_create_realm_descriptor(kvm);

	/* Create the virtual GIC. */
	if (gic__create(kvm, kvm->cfg.arch.irqchip))
		die("Failed to create virtual GIC");

	kvm__arch_enable_mte(kvm);
}

#define FDT_ALIGN	SZ_2M
#define INITRD_ALIGN	4
bool kvm__arch_load_kernel_image(struct kvm *kvm, int fd_kernel, int fd_initrd,
				 const char *kernel_cmdline)
{
	void *pos, *kernel_end, *limit;
	unsigned long guest_addr;
	unsigned long kern_offset, end_offset;
	uintptr_t initrd_end;
	ssize_t file_size, mem_size;

	/*
	 * Linux requires the initrd and dtb to be mapped inside lowmem,
	 * so we can't just place them at the top of memory.
	 */
	limit = kvm->ram_start + min(kvm->ram_size, (u64)SZ_256M) - 1;

	kern_offset = kvm__arch_get_kern_offset(kvm, fd_kernel);
	pos = kvm->ram_start + kern_offset;
	kvm->arch.kern_guest_start = host_to_guest_flat(kvm, pos);

	file_size = read_file(fd_kernel, pos, limit - pos);
	if (file_size < 0) {
		if (errno == ENOMEM)
			die("kernel image too big to contain in guest memory.");

		die_perror("kernel read");
	}

	/*
	 * If we know the actual memory size of the image, make sure we
	 * use that informatoin to avoid overlapping images
	 */
	mem_size = kvm__arch_get_image_size(kvm, pos);
	if (!mem_size)
		mem_size = file_size;
	end_offset = kern_offset + mem_size;
	pr_debug("Loaded kernel to 0x%llx - 0x%llx (%zd bytes actual)",
		 kvm->arch.kern_guest_start,
		 kvm->arch.kern_guest_start + mem_size,
		 file_size);

	kernel_end = kvm->ram_start + end_offset;
	if (kvm__is_realm(kvm)) {
		if (!IS_ALIGNED((uintptr_t)kernel_end, PAGE_SIZE))
			explicit_bzero(kernel_end,
			               (size_t)(ALIGN((uintptr_t)kernel_end, PAGE_SIZE) - (uintptr_t)kernel_end));
		kvm_arm_realm_populate_kernel(kvm, file_size, mem_size);
		/*
		 * Make sure the initrd doesn't get loaded in the tail page of
		 * the kernel.
		 */
		end_offset = ALIGN(end_offset, SZ_4K);
	}

	/*
	 * Now load backwards from the end of memory so the kernel
	 * decompressor has plenty of space to work with. First up is
	 * the device tree blob...
	 * The DTB is aligned to 2MB and thus wouldn't overlap with
	 * initrd's tail and thus we don't need any other measures
	 * for Realm.
	 */
	pos = limit;
	pos -= (FDT_MAX_SIZE + FDT_ALIGN);
	guest_addr = ALIGN(host_to_guest_flat(kvm, pos), FDT_ALIGN);
	pos = guest_flat_to_host(kvm, guest_addr);
	if (pos < kernel_end)
		die("fdt overlaps with kernel image.");

	kvm->arch.dtb_guest_start = guest_addr;
	pr_debug("Placing fdt at 0x%llx - 0x%llx",
		 kvm->arch.dtb_guest_start,
		 host_to_guest_flat(kvm, limit));
	limit = pos;

	/* ... and finally the initrd, if we have one. */
	if (fd_initrd != -1) {
		struct stat sb;

		if (fstat(fd_initrd, &sb))
			die_perror("fstat");

		pos -= (sb.st_size + INITRD_ALIGN);
		guest_addr = ALIGN(host_to_guest_flat(kvm, pos), INITRD_ALIGN);
		pos = guest_flat_to_host(kvm, guest_addr);
		if (pos < kernel_end)
			die("initrd overlaps with kernel image.");

		file_size = read_file(fd_initrd, pos, limit - pos);
		if (file_size == -1) {
			if (errno == ENOMEM)
				die("initrd too big to contain in guest memory.");

			die_perror("initrd read");
		}

		kvm->arch.initrd_guest_start = guest_addr;
		kvm->arch.initrd_size = file_size;
		pr_debug("Loaded initrd to 0x%llx (%llu bytes)",
			 kvm->arch.initrd_guest_start,
			 kvm->arch.initrd_size);

		/*
		 * kernel_end is already aligned to 4K and FDT aligned to 2M.
		 * Thus we don't run into a situation where any of these images
		 * are overlapped in a single 4K page.
		 */
		if (kvm__is_realm(kvm)) {
			if (kvm->cfg.arch.is_realm) {
				initrd_end = (uintptr_t)pos + (uintptr_t)file_size;
				if (!IS_ALIGNED(initrd_end, PAGE_SIZE))
					explicit_bzero((void *)initrd_end,
					               (size_t)(ALIGN(initrd_end, PAGE_SIZE) - initrd_end));
			}
			kvm_arm_realm_populate_initrd(kvm);
		}
	} else {
		kvm->arch.initrd_size = 0;
	}

	return true;
}

static bool validate_fw_addr(struct kvm *kvm, u64 fw_addr)
{
	u64 ram_phys;

	ram_phys = host_to_guest_flat(kvm, kvm->ram_start);

	if (fw_addr < ram_phys || fw_addr >= ram_phys + kvm->ram_size) {
		pr_err("Provide --firmware-address an address in RAM: "
		       "0x%016llx - 0x%016llx",
		       ram_phys, ram_phys + kvm->ram_size);

		return false;
	}

	return true;
}

bool kvm__load_firmware(struct kvm *kvm, const char *firmware_filename)
{
	u64 fw_addr = kvm->cfg.arch.fw_addr;
	void *host_pos, *fdt_pos;
	void *limit;
	ssize_t fw_sz, mem_sz;
	int fd;

	limit = kvm->ram_start + kvm->ram_size;

	/* For default firmware address, lets load it at the begining of RAM */
	if (fw_addr == 0)
		fw_addr = kvm->arch.memory_guest_start;

	if (!validate_fw_addr(kvm, fw_addr))
		die("Bad firmware destination: 0x%016llx", fw_addr);

	fd = open(firmware_filename, O_RDONLY);
	if (fd < 0)
		return false;

	host_pos = guest_flat_to_host(kvm, fw_addr);
	if (!host_pos || host_pos < kvm->ram_start)
		return false;

	fw_sz = read_file(fd, host_pos, limit - host_pos);
	if (fw_sz < 0)
		die("failed to load firmware");
	close(fd);

	mem_sz = kvm__arch_get_image_size(kvm, host_pos);
	if (!mem_sz)
		mem_sz = fw_sz;

	/* Kernel isn't loaded by kvm, point start address to firmware */
	kvm->arch.kern_guest_start = fw_addr;
	pr_debug("Loaded firmware to 0x%llx - 0x%llx (%zd bytes actual)",
		 kvm->arch.kern_guest_start,
		 kvm->arch.kern_guest_start + mem_sz,
		 fw_sz);

	/*
	 * FDT load address is aligned to FDT_ALIGN and it guarantees that
	 * the firmware image will not overlap the FDT in the same page for
	 * a Realm.
	 */
	if (kvm__is_realm(kvm))
		kvm_arm_realm_populate_kernel(kvm, fw_sz, mem_sz);

	host_pos += mem_sz;
	/*
	 * Load dtb at the end of the DRAM, to avoid the firmware
	 * overwriting the FDT if it overlaps with the .bss etc.
	 */
	fdt_pos = limit - (FDT_MAX_SIZE + FDT_ALIGN);
	if (fdt_pos < host_pos)
		die("not enough space to load fdt");

	kvm->arch.dtb_guest_start = ALIGN(host_to_guest_flat(kvm, fdt_pos),
					  FDT_ALIGN);
	pr_debug("Placing fdt at 0x%llx - 0x%llx",
		 kvm->arch.dtb_guest_start,
		 kvm->arch.dtb_guest_start + FDT_MAX_SIZE);

	return true;
}

int kvm__arch_setup_firmware(struct kvm *kvm)
{
	return 0;
}
