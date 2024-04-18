#include <kvm/kvm.h>
#include <channel.h>
#include <arm-common/kvm-config-arch.h>
#include <kvm/virtio-pci-dev.h>
#include <linux/byteorder.h>

static struct vchannel_device *vchannel_dev;

static int vchannel_init(struct kvm *kvm) {
    int class = VCHANNEL_PCI_CLASS_MEM;
    int ret = 0;

    pr_debug("vchannel_init start");

    if (!kvm->cfg.arch.socket_path) {
        pr_debug("vchannel_init: empty socket_path");
        return 0;
    }

    vchannel_dev = calloc(1, sizeof(*vchannel_dev));
	if (!vchannel_dev) {
        pr_debug("vchannel_init failed with -ENOMEM %d", -ENOMEM);
        return -ENOMEM;
    }

    vchannel_dev->pci_hdr = (struct pci_device_header) {
        .vendor_id = cpu_to_le16(PCI_VENDOR_ID_REDHAT_QUMRANET),
        .device_id = cpu_to_le16(VCHANNEL_PCI_DEVICE_ID),
        .header_type = PCI_HEADER_TYPE_NORMAL,
        .class[0] = class & 0xff,
        .class[1] = (class >> 8) & 0xff,
        .class[2] = (class >> 16) & 0xff,
        .subsys_vendor_id = cpu_to_le16(PCI_SUBSYSTEM_VENDOR_ID_REDHAT_QUMRANET),
        .subsys_id = cpu_to_le16(PCI_SUBSYSTEM_ID_PCI_SHMEM),
    };

    vchannel_dev->dev_hdr = (struct device_header){
        .bus_type = DEVICE_BUS_PCI,
        .data = &vchannel_dev->pci_hdr,
    };

    ret = device__register(&vchannel_dev->dev_hdr);
    if (ret < 0) {
        pr_debug("device__register failed with %d", ret);
        return ret;
    }

    pci__assign_irq(&vchannel_dev->pci_hdr);

    pr_debug("vchannel_init done successfully");

    return 0;
}
dev_base_init(vchannel_init);

static int vchannel_exit(struct kvm *kvm) {
    pr_debug("vchannel_exit start");

	if (!kvm->cfg.arch.socket_path) {
        pr_debug("vchannel_exit: empty socket_path");
        return 0;
    }

    if (vchannel_dev)
        free(vchannel_dev);

    pr_debug("vchannel_exit done successfully");

    return 0;
}
dev_base_exit(vchannel_exit);
