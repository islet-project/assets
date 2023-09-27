#include "kvm/kvm.h"
#include "kvm/kvm-cpu.h"

#include "measurement/rim-measure.h"

#include <linux/byteorder.h>
#include <asm/image.h>
#include <asm/realm.h>

static void realm_configure_hash_algo(struct kvm *kvm)
{
#ifndef RIM_MEASURE
	struct kvm_cap_arm_rme_config_item hash_algo_cfg = {
		.cfg	= KVM_CAP_ARM_RME_CFG_HASH_ALGO,
		.hash_algo = kvm->arch.measurement_algo,
	};

	struct kvm_enable_cap rme_config = {
		.cap = KVM_CAP_ARM_RME,
		.args[0] = KVM_CAP_ARM_RME_CONFIG_REALM,
		.args[1] = (u64)&hash_algo_cfg,
	};

	if (ioctl(kvm->vm_fd, KVM_ENABLE_CAP, &rme_config) < 0)
		die_perror("KVM_CAP_RME(KVM_CAP_ARM_RME_CONFIG_REALM) hash_algo");
#else
	measurer_realm_configure_hash_algo(kvm->arch.measurement_algo);
#endif
}

static void realm_configure_rpv(struct kvm *kvm)
{
#ifndef RIM_MEASURE
	struct kvm_cap_arm_rme_config_item rpv_cfg  = {
		.cfg	= KVM_CAP_ARM_RME_CFG_RPV,
	};

	struct kvm_enable_cap rme_config = {
		.cap = KVM_CAP_ARM_RME,
		.args[0] = KVM_CAP_ARM_RME_CONFIG_REALM,
		.args[1] = (u64)&rpv_cfg,
	};

	if (!kvm->cfg.arch.realm_pv)
		return;

	memset(&rpv_cfg.rpv, 0, sizeof(rpv_cfg.rpv));
	memcpy(&rpv_cfg.rpv, kvm->cfg.arch.realm_pv, strlen(kvm->cfg.arch.realm_pv));

	if (ioctl(kvm->vm_fd, KVM_ENABLE_CAP, &rme_config) < 0)
		die_perror("KVM_CAP_RME(KVM_CAP_ARM_RME_CONFIG_REALM) RPV");
#endif
}

static void realm_configure_sve(struct kvm *kvm)
{
#ifndef RIM_MEASURE
	struct kvm_cap_arm_rme_config_item sve_cfg = {
		.cfg	= KVM_CAP_ARM_RME_CFG_SVE,
		.sve_vq = kvm->arch.sve_vq,
	};

	struct kvm_enable_cap rme_config = {
		.cap = KVM_CAP_ARM_RME,
		.args[0] = KVM_CAP_ARM_RME_CONFIG_REALM,
		.args[1] = (u64)&sve_cfg,
	};

	if (kvm->cfg.arch.disable_sve)
		return;

	if (ioctl(kvm->vm_fd, KVM_ENABLE_CAP, &rme_config) < 0)
		die_perror("KVM_CAP_RME(KVM_CAP_ARM_RME_CONFIG_REALM) SVE");
#endif
}

static void realm_configure_parameters(struct kvm *kvm)
{
	realm_configure_hash_algo(kvm);
	realm_configure_rpv(kvm);
	realm_configure_sve(kvm);
}

void kvm_arm_realm_create_realm_descriptor(struct kvm *kvm)
{
#ifndef RIM_MEASURE
	struct kvm_enable_cap rme_create_rd = {
		.cap = KVM_CAP_ARM_RME,
		.args[0] = KVM_CAP_ARM_RME_CREATE_RD,
	};
#endif
	realm_configure_parameters(kvm);

#ifndef RIM_MEASURE
	if (ioctl(kvm->vm_fd, KVM_ENABLE_CAP, &rme_create_rd) < 0)
		die_perror("KVM_CAP_RME(KVM_CAP_ARM_RME_CREATE_RD)");
#endif

	measurer_kvm_arm_realm_create_realm_descriptor();
}

static void realm_init_ipa_range(struct kvm *kvm, u64 start, u64 size)
{
#ifndef RIM_MEASURE
	struct kvm_cap_arm_rme_init_ipa_args init_ipa_args = {
		.init_ipa_base = start,
		.init_ipa_size = size
	};
	struct kvm_enable_cap rme_init_ipa_realm = {
		.cap = KVM_CAP_ARM_RME,
		.args[0] = KVM_CAP_ARM_RME_INIT_IPA_REALM,
		.args[1] = (u64)&init_ipa_args
	};

	if (ioctl(kvm->vm_fd, KVM_ENABLE_CAP, &rme_init_ipa_realm) < 0)
		die("unable to intialise IPA range for Realm %llx - %llx (size %llu)",
		    start, start + size, size);
#else
	measurer_realm_init_ipa_range(start, start + size);
#endif
}

static void __realm_populate(struct kvm *kvm, u64 start, u64 size)
{
#ifndef RIM_MEASURE

	struct kvm_cap_arm_rme_populate_realm_args populate_args = {
		.populate_ipa_base = start,
		.populate_ipa_size = size
	};
	struct kvm_enable_cap rme_populate_realm = {
		.cap = KVM_CAP_ARM_RME,
		.args[0] = KVM_CAP_ARM_RME_POPULATE_REALM,
		.args[1] = (u64)&populate_args
	};

	if (ioctl(kvm->vm_fd, KVM_ENABLE_CAP, &rme_populate_realm) < 0)
		die("unable to populate Realm memory %llx - %llx (size %llu)",
		    start, start + size, size);
#else
	measurer_realm_populate(kvm, start, start + size);
#endif
}

static void realm_populate(struct kvm *kvm, u64 start, u64 size)
{
	realm_init_ipa_range(kvm, start, size);
	__realm_populate(kvm, start, size);
}

static bool is_arm64_linux_kernel_image(void *header)
{
	struct arm64_image_header *hdr = header;

	return memcmp(&hdr->magic, ARM64_IMAGE_MAGIC, sizeof(hdr->magic)) == 0;
}

static ssize_t arm64_linux_kernel_image_size(void *header)
{
	struct arm64_image_header *hdr = header;

	if (is_arm64_linux_kernel_image(header))
		return le64_to_cpu(hdr->image_size);
	die("Not arm64 Linux kernel Image");
}

void kvm_arm_realm_populate_kernel(struct kvm *kvm)
{
	u64 start, end, mem_size;
	void *header = guest_flat_to_host(kvm, kvm->arch.kern_guest_start);

	start = ALIGN_DOWN(kvm->arch.kern_guest_start, SZ_4K);
	end = ALIGN(kvm->arch.kern_guest_start + kvm->arch.kern_size, SZ_4K);

	if (is_arm64_linux_kernel_image(header))
		mem_size = arm64_linux_kernel_image_size(header);
	else
		mem_size = end - start;

	realm_init_ipa_range(kvm, start, mem_size);
	__realm_populate(kvm, start, end - start);
}

void kvm_arm_realm_populate_initrd(struct kvm *kvm)
{
	u64 kernel_end, start, end;

	kernel_end = ALIGN(kvm->arch.kern_guest_start + kvm->arch.kern_size, SZ_4K);
	start = ALIGN_DOWN(kvm->arch.initrd_guest_start, SZ_4K);
	/*
	 * Because we align the initrd to 4 bytes, it is theoretically possible
	 * for the start of the initrd to overlap with the same page where the
	 * kernel ends.
	 */
	if (start < kernel_end)
		start = kernel_end;
	end = ALIGN(kvm->arch.initrd_guest_start + kvm->arch.initrd_size, SZ_4K);
	if (end > start)
		realm_populate(kvm, start, end - start);
}

void kvm_arm_realm_populate_dtb(struct kvm *kvm)
{
	u64 initrd_end, start, end;

	initrd_end = ALIGN(kvm->arch.initrd_guest_start + kvm->arch.initrd_size, SZ_4K);
	start = ALIGN_DOWN(kvm->arch.dtb_guest_start, SZ_4K);
	/*
	 * Same situation as with the initrd, but now it is the DTB which is
	 * overlapping with the last page of the initrd, because the initrd is
	 * populated first.
	 */
	if (start < initrd_end)
		start = initrd_end;
	end = ALIGN(kvm->arch.dtb_guest_start + FDT_MAX_SIZE, SZ_4K);
	if (end > start)
		realm_populate(kvm, start, end - start);
}

static void kvm_arm_realm_activate_realm(struct kvm *kvm)
{
#ifndef RIM_MEASURE
	struct kvm_enable_cap activate_realm = {
		.cap = KVM_CAP_ARM_RME,
		.args[0] = KVM_CAP_ARM_RME_ACTIVATE_REALM,
	};

	if (ioctl(kvm->vm_fd, KVM_ENABLE_CAP, &activate_realm) < 0)
		die_perror("KVM_CAP_ARM_RME(KVM_CAP_ARM_RME_ACTIVATE_REALM)");

	kvm->arch.realm_is_active = true;
#else
	measurer_print_rim();
	exit(0);
#endif
}

static int kvm_arm_realm_finalize(struct kvm *kvm)
{
	int i;

	if (!kvm->cfg.arch.is_realm)
		return 0;

	/*
	 * VCPU reset must happen before the realm is activated, because their
	 * state is part of the cryptographic measurement for the realm.
	 */
	for (i = 0; i < kvm->nrcpus; i++)
		kvm_cpu__reset_vcpu(kvm->cpus[i]);

	/* Activate and seal the measurement for the realm. */
	kvm_arm_realm_activate_realm(kvm);

	return 0;
}
last_init(kvm_arm_realm_finalize)
