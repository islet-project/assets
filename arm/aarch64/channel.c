#include <kvm/kvm.h>
#include <channel.h>
#include <arm-common/kvm-config-arch.h>
#include <kvm/virtio-pci-dev.h>
#include <linux/byteorder.h>
#include <socket.h>
#include <linux/types.h>

static struct vchannel_device *vchannel_dev; // for channel module in current realm

static int vchannel_init(struct kvm *kvm) {
    int class = VCHANNEL_PCI_CLASS_MEM;
    int ret = 0;
    Client *client;
    u32 ioeventfd_addr = IOEVENTFD_BASE_ADDR;

    // TODO: need to open host channel module and send its eventfd

    ch_syslog("%s start", __func__);
    ch_syslog("vchannel ioeventfd_addr: 0x%x", ioeventfd_addr);

    if (!kvm->cfg.arch.socket_path) {
        ch_syslog("vchannel_init: empty socket_path");
        return 0;
    }

    // Setup virtual PCI device 
    vchannel_dev = calloc(1, sizeof(*vchannel_dev));
	if (!vchannel_dev) {
        ch_syslog("vchannel_init failed with -ENOMEM %d", -ENOMEM);
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

    ch_syslog("vchannel vendor_id: 0x%x, device_id: 0x%x",
			vchannel_dev->pci_hdr.vendor_id, vchannel_dev->pci_hdr.device_id);

    vchannel_dev->dev_hdr = (struct device_header){
        .bus_type = DEVICE_BUS_PCI,
        .data = &vchannel_dev->pci_hdr,
    };

    ret = device__register(&vchannel_dev->dev_hdr);
    if (ret < 0) {
        ch_syslog("device__register failed with %d", ret);
        return ret;
    }

	vchannel_dev->pci_hdr.irq_type = IRQ_TYPE_EDGE_RISING;
    pci__assign_irq(&vchannel_dev->pci_hdr);
    vchannel_dev->gsi = vchannel_dev->pci_hdr.irq_line - KVM_IRQ_OFFSET;

	ch_syslog("irq_type %d", vchannel_dev->pci_hdr.irq_type);

    // Setup client
    client = get_client(kvm->cfg.arch.socket_path, ioeventfd_addr, kvm);
    if (!client || !client->initialized) {
        ch_syslog("failed to get client");
        return -EINVAL;
    }

    if (!is_valid_shm_id(client, kvm->cfg.arch.shm_id)) {
        ch_syslog("[ID:%d] shm_id expect %d but current shm_id: %d",
                 client->id, client->shm_id, kvm->cfg.arch.shm_id);
        return -EINVAL;
    } else {
        ch_syslog("[ID:%d] Create pthread for socket fd polling", client->id);
        ret = pthread_create(&client->thread, NULL, poll_events, (void *)client);
        if (ret) {
            close_client(client);
            die("failed to create a thread with poll_events()");
        }
        ch_syslog("[ID:%d] pthread_create returns %d", client->id, ret);

        ret = pthread_detach(client->thread);
        if (ret) {
            close_client(client);
            die("failed to detach a thread");
        }
        usleep(100000);
    }

    ch_syslog("[ID:%d] request irq__add_irqfd gsi %d fd %d",
            client->id, vchannel_dev->gsi, client->eventfd);
    // Setup notification channel to receive as a guest interrupt
    ret = irq__add_irqfd(kvm, vchannel_dev->gsi, client->eventfd, 0);

    ch_syslog("vchannel_init done successfully");

    return 0;
}
dev_base_init(vchannel_init);

static int vchannel_exit(struct kvm *kvm) {
    ch_syslog("vchannel_exit start");

	if (!kvm->cfg.arch.socket_path) {
        ch_syslog("vchannel_exit: empty socket_path");
        return 0;
    }

    if (vchannel_dev)
        free(vchannel_dev);

    ch_syslog("vchannel_exit done successfully");

    return 0;
}
dev_base_exit(vchannel_exit);
