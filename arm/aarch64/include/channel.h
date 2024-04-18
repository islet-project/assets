#ifndef ARM_AARCH64__CHANNEL_H
#define ARM_AARCH64__CHANNEL_H

#include <kvm/pci.h>
#include <kvm/devices.h>

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
#define VCHANNEL_PCI_DEVICE_ID  0x10f0
#define VCHANNEL_PCI_CLASS_MEM 0x050000

struct vchannel_device {
	struct pci_device_header	pci_hdr;
	struct device_header		dev_hdr;
	int gsi;
	int fd;
};

#endif // ARM_AARCH64__CHANNEL_H
