#include <kvm/kvm.h>
#include <channel.h>
#include <arm-common/kvm-config-arch.h>
#include <kvm/virtio-pci-dev.h>
#include <linux/byteorder.h>
#include <socket.h>
#include <linux/types.h>
#include <asm/realm.h>

static struct vchannel_device *vchannel_dev; // for channel module in current realm
static u64 shm_ipa_base = INTER_REALM_SHM_IPA_BASE;

static void* request_memory_to_host_channel(void) {
    void *mem = 0;
    int hc_fd;

    hc_fd = open(HOST_CHANNEL_PATH, O_RDWR);
	if (hc_fd < 0) {
		ch_syslog("failed to open %s: %d\n", HOST_CHANNEL_PATH, hc_fd);
		return mem;
	}

	ch_syslog("mmap with PROT_READ");
	mem = mmap(NULL, INTER_REALM_SHM_SIZE, PROT_RW,
			MAP_SHARED | MAP_NORESERVE | MAP_LOCKED, hc_fd, 0);
	close(hc_fd);

	if (mem == MAP_FAILED) {
		ch_syslog("failed to mmap %s: %d\n", HOST_CHANNEL_PATH, strerror(errno));
		return 0;
	}
	return mem;
}

static int setup_shared_memory(struct kvm *kvm) {
    int ret = 0;
    void *mem;

    mem = request_memory_to_host_channel();
    if (!mem) {
        return -1;
    }

    ret = kvm__register_ram(kvm, shm_ipa_base, INTER_REALM_SHM_SIZE, mem);
    if (ret) {
		munmap(mem, INTER_REALM_SHM_SIZE);
		ch_syslog("[KVMTOOL] %s failed with %d", __func__, ret);
		return ret;
	}

    kvm_arm_realm_populate_shared_mem(kvm, shm_ipa_base, INTER_REALM_SHM_SIZE);
    shm_ipa_base += INTER_REALM_SHM_SIZE;
	ch_syslog("[KVMTOOL] %s updated shm_ipa_base 0x%llx", __func__, shm_ipa_base);

    return ret;
}

int allocate_shm_after_realm_activate(struct kvm *kvm) {
	void* mem;
	int ret = 0;

	ch_syslog("[KVMTOOL] %s start", __func__);
    mem = request_memory_to_host_channel();
    if (!mem) {
        return -1;
    }

	ch_syslog("[KVMTOOL] %s shm_ipa_base 0x%llx", __func__, shm_ipa_base);

    ret = kvm__register_ram(kvm, shm_ipa_base, INTER_REALM_SHM_SIZE, mem);
    if (ret) {
		munmap(mem, INTER_REALM_SHM_SIZE);
		ch_syslog("[KVMTOOL] %s failed with %d", __func__, ret);
		return ret;
	}

	// TODO: call DATA_CREATE_UNKNOWN RMI CALL
    map_memory_to_realm(kvm, (u64)mem, shm_ipa_base, INTER_REALM_SHM_SIZE);

    shm_ipa_base += INTER_REALM_SHM_SIZE;
	ch_syslog("[KVMTOOL] %s updated shm_ipa_base 0x%llx", __func__, shm_ipa_base);

	ch_syslog("[KVMTOOL] %s done: [%p:%p]", __func__, mem, mem + INTER_REALM_SHM_SIZE);
	return ret;
}

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
            die("vchannel_init: failed to create a thread with poll_events()");
        }
        ch_syslog("[ID:%d] pthread_create returns %d", client->id, ret);

        ret = pthread_detach(client->thread);
        if (ret) {
            close_client(client);
            die("vchannel_init: failed to detach a thread");
        }
        usleep(100000);
    }
    // Set first shared memory
    ret = setup_shared_memory(kvm);
    if (ret) {
        die("vchannel_init: setup_shared_memory is failed");
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
