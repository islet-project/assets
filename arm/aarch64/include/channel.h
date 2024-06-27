#ifndef ARM_AARCH64__CHANNEL_H
#define ARM_AARCH64__CHANNEL_H

#include <kvm/pci.h>
#include <kvm/devices.h>
#include <syslog.h>
#include <stdarg.h>
#include <kvm/kvm.h>
#include <socket.h>

/*
 * The device id should be included in the following range to avoid conflict with other device ids:
 * 
 * 1af4:10f0 to 1a4f:10ff
 * Available for experimental usage without registration.
 * Must get official ID when the code leaves the test lab
 * (i.e. when seeking upstream merge or shipping a distro/product) to avoid conflicts.
 * 
 * Referenced by https://github.com/qemu/qemu/blob/master/docs/specs/pci-ids.rst
 */
#define VCHANNEL_PCI_DEVICE_ID 0x1110 // temporarily uses ivshmem's device id
#define VCHANNEL_PCI_CLASS_MEM 0x050000

#define IOEVENTFD_BASE_SIZE 0x100
#define IOEVENTFD_BASE_ADDR (KVM_PCI_MMIO_AREA + ARM_PCI_MMIO_SIZE - IOEVENTFD_BASE_SIZE) // use end address of KVM_PCI_MMIO_AREA


#define BAR_MMIO_PEER_VMID 0
#define BAR_MMIO_SHM_RW_IPA_BASE 32 // 4 byte
#define BAR_MMIO_SHM_RO_IPA_BASE 64 // 8 byte

#define SYSLOG_PREFIX "KVMTOOL"

#define PAGE_MASK (~(PAGE_SIZE - 1))
#define MMAP_OWNER_VMID_MASK 0xFF
#define MMAP_SHARE_OTHER_REALM_MEM_MASK 0x100

struct channel_ioctl_info {
	__u64 owner_vmid;
	__u64 shm_pa;
};

#define CHANNEL_IO 0xC
#define CH_GET_SHM_PA _IOWR(CHANNEL_IO, 0x1, struct channel_ioctl_info)

struct vchannel_device {
	struct pci_device_header	pci_hdr;
	struct device_header		dev_hdr;

	int gsi;
};

static void ch_syslog(const char *format, ...) {
	va_list args;

	// Write logs to the default syslog path (e.g, /var/log/syslog) & stderr as well
	openlog(SYSLOG_PREFIX, LOG_CONS | LOG_PID | LOG_PERROR, LOG_USER);

	va_start(args, format);
	vsyslog(LOG_INFO, format, args);
	va_end(args);

	closelog();
}

int allocate_shm_after_realm_activate(Client *client, int vmid, bool need_allocated_mem, u64 other_shrm_ipa);

#endif // ARM_AARCH64__CHANNEL_H
