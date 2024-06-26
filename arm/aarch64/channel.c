#include <channel.h>
#include <arm-common/kvm-config-arch.h>
#include <kvm/virtio-pci-dev.h>
#include <linux/byteorder.h>
#include <socket.h>
#include <linux/types.h>
#include <asm/realm.h>
#include <kvm/ioport.h>
#include <assert.h>

static struct vchannel_device *vchannel_dev; // for channel module in current realm
static u64 shm_ipa_base = INTER_REALM_SHM_RW_IPA_START;
static u64 shm_ro_ipa_base = INTER_REALM_SHM_RO_IPA_START;
static u64 *shm_ro_userspace_va;

static void* request_memory_to_host_channel(int vmid, bool need_allocated_mem) {
    void *mem = 0;
    int hc_fd;
    u64 offset = vmid & MMAP_OWNER_VMID_MASK;

    // Request already allocated peer vmid's memory
    if (need_allocated_mem) {
        offset |= MMAP_SHARE_OTHER_REALM_MEM_MASK;
    }

    hc_fd = open(HOST_CHANNEL_PATH, O_RDWR);
	if (hc_fd < 0) {
		ch_syslog("failed to open %s: %d\n", HOST_CHANNEL_PATH, hc_fd);
		return mem;
	}

    offset = offset << 12;
    ch_syslog("offset: 0x%llx\n", offset);

    mem = mmap(NULL, INTER_REALM_SHM_SIZE, PROT_RW,
			MAP_SHARED | MAP_NORESERVE | MAP_LOCKED, hc_fd, offset);
	close(hc_fd);

	if (mem == MAP_FAILED) {
		ch_syslog("failed to mmap %s: %s\n", HOST_CHANNEL_PATH, strerror(errno));
		return 0;
	}
	return mem;
}

static int setup_shared_memory_before_realm_activate(struct kvm *kvm, int target_vmid, bool need_allocated_mem) {
    int ret = 0;
    void *mem;
    u64 *test_ptr;
    u64 test_val = 0xABCD;
    u64 *ipa = (need_allocated_mem) ? &shm_ro_ipa_base : &shm_ipa_base;

    mem = request_memory_to_host_channel(target_vmid, need_allocated_mem);
    if (!mem) {
        return -1;
    }

    ch_syslog("[KVMTOOL] write test before registering this memory to realm. mem user va: 0x%llx", mem);
    test_ptr = (u64 *)mem;

    ch_syslog("[KVMTOOL] [TEST] Let's write 0x%llx to the mem: 0x%llx", test_val, test_ptr);
    ch_syslog("[KVMTOOL] [TEST] before writing a value to the mem: 0x%llx", *test_ptr);
    *test_ptr = test_val;
    ch_syslog("[KVMTOOL] [TEST] after writing a value to the mem: 0x%llx", *test_ptr);

    ret = kvm__register_ram(kvm, *ipa, INTER_REALM_SHM_SIZE, mem);
    if (ret) {
		munmap(mem, INTER_REALM_SHM_SIZE);
		ch_syslog("[KVMTOOL] %s failed with %d", __func__, ret);
		return ret;
	}

    kvm_arm_realm_populate_shared_mem(kvm, *ipa, INTER_REALM_SHM_SIZE);
    *ipa += INTER_REALM_SHM_SIZE;
	ch_syslog("[KVMTOOL] %s updated ipa 0x%llx", __func__, *ipa);

    return ret;
}

static int setup_first_shared_memory(struct kvm *kvm, int cur_vmid) {
    return setup_shared_memory_before_realm_activate(kvm, cur_vmid, false);
}

int allocate_shm_after_realm_activate(struct kvm *kvm, int target_vmid, bool need_ro_mem) {
	int ret = 0;
	void* mem;
    u64* ipa = (need_ro_mem) ? &shm_ro_ipa_base : &shm_ipa_base;

	ch_syslog("[KVMTOOL] %s start", __func__);
    mem = request_memory_to_host_channel(target_vmid, need_ro_mem);
    if (!mem) {
        return -1;
    }

    shm_ro_userspace_va = mem;
    ch_syslog("[KVMTOOL] %s ipa 0x%llx, shm_ro_userspace_va 0x%llx", __func__, *ipa, shm_ro_userspace_va);

    ret = kvm__register_ram(kvm, *ipa, INTER_REALM_SHM_SIZE, mem);
    if (ret) {
		munmap(mem, INTER_REALM_SHM_SIZE);
		ch_syslog("[KVMTOOL] %s failed with %d", __func__, ret);
		return ret;
	}

	// TODO: call DATA_CREATE_UNKNOWN RMI CALL
    map_memory_to_realm(kvm, (u64)mem, *ipa, INTER_REALM_SHM_SIZE);

    *ipa += INTER_REALM_SHM_SIZE;
	ch_syslog("[KVMTOOL] %s updated ipa 0x%llx", __func__, *ipa);

	ch_syslog("[KVMTOOL] %s done: [%p:%p]", __func__, mem, mem + INTER_REALM_SHM_SIZE);
	return ret;
}

static void vchannel_mmio_callback(struct kvm_cpu *vcpu, u64 addr,
					 u8 *data, u32 len, u8 is_write,
					 void *ptr)
{
	Client* client = ptr;
	u64 mmio_addr;
    u64 offset;
    u64 test_val = 0xffff;

    ch_syslog("%s start. is_write %d", __func__, is_write);
    mmio_addr = pci__bar_address(&vchannel_dev->pci_hdr, 0);
	ch_syslog("%s bar 0 addr 0x%x, trapped addr 0x%llx", __func__, mmio_addr, addr);
    offset = addr - mmio_addr;

    if (is_write) {
        //ch_syslog("%s [TEST] Write 0x%llx to the realm shm_ro_userspace_va: 0x%llx", __func__, test_val, shm_ro_userspace_va);
        ch_syslog("%s [TEST] Read the realm shm_ro_userspace_va: 0x%llx", __func__, shm_ro_userspace_va);
    }

    if (!client->peer_cnt) {
        ioport__write32((u32*)data, INVALID_PEER_ID); // allows only the first peer
        ch_syslog("%s there is no peer. write INVALID_PEER_ID: %d", __func__, INVALID_PEER_ID);
        return;
    }

    if (offset == BAR_MMIO_OFFSET_PEER_VMID) {
        ioport__write32((u32*)data, client->peers[0].id); // allows only the first peer
        ch_syslog("%s write destination realm VMID %d", __func__, client->peers[0].id);
    } else if (offset == BAR_MMIO_OFFSET_SHM_RO_IPA_BASE) {
        u64 shm_ro_ipa = shm_ro_ipa_base;

        if (!is_write) {
            allocate_shm_after_realm_activate(client->kvm, client->peers[0].id, true);

            ioport__write32((u32 *)data, shm_ro_ipa);
            ch_syslog("%s write shm_ro_ipa 0x%llx", __func__, shm_ro_ipa);
        } else {
            ch_syslog("%s [TEST] It should get panic because The host doesn't have any permission to access the memory.", __func__);
            test_val = *shm_ro_userspace_va;
            //*shm_ro_userspace_va = 0xDEAD;
            ch_syslog("%s [TEST] shouldn't reach here. something's wrong 0x%llx", __func__, test_val);
        }
    } else {
        ch_syslog("%s wrong offset %d", __func__, offset);
    }
}

static int vchannel_pci__bar_activate(struct kvm *kvm,
				    struct pci_device_header *pci_hdr,
				    int bar_num, void *data)
{
	u32 bar_addr, bar_size;
	int ret = -EINVAL;

	assert(bar_num == 0);

	bar_addr = pci__bar_address(pci_hdr, bar_num);
	bar_size = pci__bar_size(pci_hdr, bar_num);

	ch_syslog("%s bar_addr 0x%x", __func__, bar_addr);

    ret = kvm__register_mmio(kvm, bar_addr, bar_size, false, &vchannel_mmio_callback, data);

    return ret;
}

static int vchannel_pci__bar_deactivate(struct kvm *kvm,
				      struct pci_device_header *pci_hdr,
				      int bar_num, void *data)
{
	u32 bar_addr;
	int ret = -EINVAL;

	assert(bar_num == 0);

	bar_addr = pci__bar_address(pci_hdr, bar_num);
	ch_syslog("%s bar_addr 0x%x", __func__, bar_addr);

    ret = kvm__deregister_mmio(kvm, bar_addr);
    /* kvm__deregister_mmio fails when the region is not found. */
    return ret ? 0 : -ENOENT;
}

static int vchannel_init(struct kvm *kvm) {
    int class = VCHANNEL_PCI_CLASS_MEM;
    int ret = 0;
	u32 mmio_addr = pci_get_mmio_block(PCI_IO_SIZE);
    Client* client = get_client(kvm->cfg.arch.socket_path, IOEVENTFD_BASE_ADDR, kvm);

    ch_syslog("%s start", __func__);

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
		.bar[0]			= cpu_to_le32(mmio_addr
							| PCI_BASE_ADDRESS_SPACE_MEMORY),
		.bar_size[0]		= cpu_to_le32(PCI_IO_SIZE),
    };

	ret = pci__register_bar_regions(kvm, &vchannel_dev->pci_hdr,
				      vchannel_pci__bar_activate,
				      vchannel_pci__bar_deactivate, client);

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
    
    // Set first shared memory
    ret = setup_first_shared_memory(kvm, client->vmid);
    if (ret) {
        die("vchannel_init: setup_first_shared_memory is failed");
    }

	ret = set_ioeventfd(client, client->shm_alloc_efd, SHM_ALLOC_EFD_ID);

    ch_syslog("[ID:%d] request irq__add_irqfd gsi %d fd %d",
              client->vmid, vchannel_dev->gsi, client->eventfd);

    // Setup notification channel to receive as a guest interrupt
    ret = irq__add_irqfd(kvm, vchannel_dev->gsi, client->eventfd, 0);

    create_polling_thread(client);

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
