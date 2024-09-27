#include <channel.h>
#include <arm-common/kvm-config-arch.h>
#include <kvm/virtio-pci-dev.h>
#include <linux/byteorder.h>
#include <linux/types.h>
#include <asm/realm.h>
#include <kvm/ioport.h>
#include <assert.h>

static struct vchannel_device *vchannel_dev; // for channel module in current realm

static void* request_memory_to_host_channel(int vmid, SHRM_TYPE shrm_type, u64* shrm_id) {
    void *mem = 0;
    int hc_fd;
    u64 offset = 0;
    int buf = -1;

    if (!shrm_id) {
        pr_err("%s: shrm_id shouldn't be null", __func__);
        return NULL;
    }

    // Request already allocated peer vmid's memory
    if (shrm_type == SHRM_RO) {
        offset |= MMAP_SHARE_OTHER_REALM_MEM_MASK;
        offset |= *shrm_id << MMAP_SHRM_ID_SHIFT;
    }
    offset |= vmid & MMAP_OWNER_VMID_MASK;

    hc_fd = open(HOST_CHANNEL_PATH, O_RDWR);
	if (hc_fd < 0) {
		ch_syslog("failed to open %s: %d\n", HOST_CHANNEL_PATH, hc_fd);
		return mem;
	}

    offset = offset << 12;
    ch_syslog("mmap offset: 0x%llx\n", offset);

    mem = mmap(NULL, INTER_REALM_SHM_SIZE, PROT_RW,
			MAP_SHARED | MAP_NORESERVE | MAP_LOCKED, hc_fd, offset);

	if (mem == MAP_FAILED) {
		pr_err("failed to mmap %s: %s\n", HOST_CHANNEL_PATH, strerror(errno));
        goto err;
    }

    if (shrm_type == SHRM_RW) {
        buf = vmid;
        if (write(hc_fd, &buf, sizeof(buf)) < 0) { //TODO: Test it !
            perror("write error");
            goto err;
        }

        if (buf < 0) {
            pr_err("%s: failed to find SHRM_RW", __func__);
            goto err;
        }

        *shrm_id = buf;
        ch_syslog("%s: returned shrm_id %llx", __func__, *shrm_id);
    }

    close(hc_fd);
	return mem;

err:
    close(hc_fd);
	return NULL;
}

int alloc_shared_realm_memory(Client *client, int target_vmid, SHRM_TYPE shrm_type, u64 shrm_id) {
	int ret = 0;
	void* mem;
    struct shared_realm_memory *shrm;
    u64 ipa = get_unmapped_ipa(shrm_type);

    ch_syslog("[KVMTOOL] %s start vmid %d, ipa 0x%llx, shrm_type %d, shrm_id %d",
              __func__, target_vmid, ipa, shrm_type, shrm_id);
    mem = request_memory_to_host_channel(target_vmid, shrm_type, &shrm_id);
    if (!mem) {
        return -1;
    }

    shrm = malloc(sizeof(struct shared_realm_memory));
    shrm->owner_vmid = target_vmid;
    shrm->shrm_id = shrm_id;
    shrm->ipa = ipa;
    shrm->va = (u64)mem;

    /*
     * If the type is SHRM_RO, then it means the shrm was already mapped to the owner realm
     * and the owner shared it to the peer realm using I/O ring.
     * That's why the current realm knows the shrm_id in function argument
     */
    shrm->mapped_to_owner_realm = (shrm_type == SHRM_RO) ? true : false;
    shrm->mapped_to_peer = false;
    list_add_tail(&shrm->list, &client->dyn_shrms_head);
    ch_syslog("%s list_add_tail: shrm->list: %p, va: 0x%llx, ipa: 0x%llx shrm_id %d",
              __func__, &shrm->list, shrm->va, shrm->ipa, shrm->shrm_id);

    ret = kvm__register_ram(client->kvm, ipa, INTER_REALM_SHM_SIZE, mem);
    if (ret) {
		munmap(mem, INTER_REALM_SHM_SIZE);
		ch_syslog("[KVMTOOL] %s failed with %d", __func__, ret);
		return ret;
	}

    shared_data_create(client->kvm, (u64)mem, ipa, INTER_REALM_SHM_SIZE, (bool)shrm_type);
    set_ipa_bit(ipa);

    ch_syslog("[KVMTOOL] %s updated ipa 0x%llx", __func__, ipa);

	ch_syslog("[KVMTOOL] %s done: [%p:%p]", __func__, mem, mem + INTER_REALM_SHM_SIZE);
	return ret;
}

static int _free_shrm(Client *client, struct shared_realm_memory* shrm, bool unmap_only) {
    int ret = 0;

    shared_data_destroy(client->kvm, shrm->va, shrm->ipa, INTER_REALM_SHM_SIZE);

    ret = kvm__destroy_mem(client->kvm, shrm->ipa, INTER_REALM_SHM_SIZE, (u64*)shrm->va);
    if (ret) {
		ch_syslog("[KVMTOOL] %s failed with %d", __func__, ret);
		return ret;
	}

    munmap((u64*)shrm->va, INTER_REALM_SHM_SIZE);
    ch_syslog("[KVMTOOL] %s munmap 0x%llx done", __func__, shrm->va);
    ch_syslog("%s list_del: shrm->list: %p, va: 0x%llx, ipa: 0x%llx",
              __func__, &shrm->list, shrm->va, shrm->ipa);
    list_del(&shrm->list);
    free(shrm);
    ch_syslog("[KVMTOOL] %s done", __func__);

    return ret;
}

/* unmap_only:
 * - false: call RMI::DATA_DESTROY & UNDELEGATE
 * - true: call RMI::UNMAP_SHARED_REALM_MEM only
 */
static int free_shrm(Client *client, int owner_vmid, u64 ipa, bool unmap_only) {
    int ret = 0;
    struct shared_realm_memory *shrm, *target = NULL;
    ch_syslog("%s Free the ipa from dyn_shrm_head", __func__);

    list_for_each_entry(shrm, &client->dyn_shrms_head, list) {
        ch_syslog("%s &shrm->list %p, &client->dyn_shrms_head %p", __func__, &shrm->list, &client->dyn_shrms_head);
        ch_syslog("%s shrm->ipa 0x%llx", __func__, shrm->ipa);
        if (shrm->ipa == ipa && shrm->owner_vmid == owner_vmid) {
            if (!shrm->mapped_to_owner_realm) {
                pr_err("%s can't free ipa 0x%llx. It is not mapped to realm", __func__, ipa);
                return -1;
            }
            target = shrm;
        }
    }

    if (target) {
        ret = _free_shrm(client, target, unmap_only);
        clear_ipa_bit(ipa);
    }

    return ret;
}

static void vchannel_mmio_callback(struct kvm_cpu *vcpu, u64 addr,
					 u8 *data, u32 len, u8 is_write,
					 void *ptr)
{
    struct shared_realm_memory *shrm;
	Client* client = ptr;
    int ret = 0;
    u64 mmio_addr;
    u64 offset;
    u64 shrm_rw_ipa = 0;
    u64 shrm_ro_ipa = 0;
    u64 shrm_id = 0;

    ch_syslog("%s start. is_write %d", __func__, is_write);
    mmio_addr = pci__bar_address(&vchannel_dev->pci_hdr, 0);
	ch_syslog("%s bar 0 addr 0x%x, trapped addr 0x%llx", __func__, mmio_addr, addr);
    offset = addr - mmio_addr;

    switch(offset) {
        case BAR_MMIO_CURRENT_VMID:
            ioport__write32((u32 *)data, client->vmid); // allows only the first peer
            ch_syslog("%s write current realm VMID %d", __func__, client->vmid);
            break;
        case BAR_MMIO_PEER_VMID:
            if (!client->peer_cnt) {
                ioport__write32((u32 *)data, INVALID_PEER_ID); // allows only the first peer
                ch_syslog("%s there is no peer. write INVALID_PEER_ID: %d", __func__, INVALID_PEER_ID);
                return;
            }

            ioport__write32((u32 *)data, client->peers[0].id); // allows only the first peer
            ch_syslog("%s write destination realm VMID %d", __func__, client->peers[0].id);
            break;
        case BAR_MMIO_SHM_RW_IPA_BASE:
            if (is_write) {
                shrm_rw_ipa = ioport__read32((u32 *)data);
                ret = free_shrm(client, client->vmid, shrm_rw_ipa, false);
                if (ret)
                    pr_err("%s free_shrm failed with %d", __func__, ret);

                return;
            }

            list_for_each_entry(shrm, &client->dyn_shrms_head, list) {
                if (!shrm->mapped_to_owner_realm) {
                    shrm_rw_ipa = shrm->ipa;
                    shrm_id = shrm->shrm_id;
                    shrm->mapped_to_owner_realm = true;
                    break;
                }
            }

            ch_syslog("%s BAR_MMIO_SHM_RW_IPA_BASE ioport__write shrm_rw_ipa 0x%llx shrm_id %d",
            __func__, shrm_rw_ipa, shrm_id);
            ioport__write32((u32 *)data, shrm_rw_ipa | shrm_id);
            ch_syslog("%s BAR_MMIO_SHM_RW_IPA_BASE done", __func__);
            break;
        case BAR_MMIO_SHM_RO_IPA_BASE:
            if (is_write) {
                shrm_id = ioport__read32((u32 *)data);
                ch_syslog("%s write on BAR_MMIO_SHM_RO_IPA_BASE with shrm_id 0x%llx", __func__, shrm_id);
                alloc_shared_realm_memory(client, client->peers[0].id, SHRM_RO, shrm_id);
            } else {
                ch_syslog("%s read on BAR_MMIO_SHM_RO_IPA_BASE", __func__);
                shrm_ro_ipa = 0;
                shrm_id = 0;

                list_for_each_entry(shrm, &client->dyn_shrms_head, list) {
                    if (shrm->mapped_to_owner_realm && !shrm->mapped_to_peer && client->peers[0].id == shrm->owner_vmid) {
                        shrm_ro_ipa = shrm->ipa;
                        shrm_id = shrm->shrm_id;
                        shrm->mapped_to_peer = true;
                        break;
                    }
                }

                if (!shrm_ro_ipa || !shrm_id) {
                    pr_err("%s: failed to get shrm_ro", __func__);
                }
                ch_syslog("%s: shrm_ro_ipa %llx, shrm_id %d", __func__, shrm_ro_ipa, shrm_id);
                ioport__write32((u32 *)data, shrm_ro_ipa | shrm_id);
            }
            break;
        case BAR_MMIO_UNMAP_SHRM_IPA:
            if (is_write) {
                shrm_ro_ipa = ioport__read32((u32 *)data);
                ch_syslog("%s write on BAR_MMIO_UNMAP_SHRM_IPA with ipa 0x%llx", __func__, shrm_ro_ipa);
                ret = free_shrm(client, client->peers[0].id, shrm_ro_ipa, true);
                if (ret)
                    pr_err("%s free_shrm failed with %d", __func__, ret);

                return;
            } else {
                pr_err("%s read on BAR_MMIO_UNMAP_SHRM_IPA is not supported", __func__);
            }
            //TODO: add a new offset to unmap RO shrm
        default:
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
    return ret ? 0 :
                -ENOENT;
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

	ret = set_ioeventfd(client, client->shm_alloc_efd, SHM_ALLOC_EFD_ID);

    ch_syslog("[ID:%d] request irq__add_irqfd gsi %d fd %d",
              client->vmid, vchannel_dev->gsi, client->eventfd);

    // Setup notification channel to receive as a guest interrupt
    ret = irq__add_irqfd(kvm, vchannel_dev->gsi, client->eventfd, 0);

    create_polling_thread(client);

    //TODO: if peer exist, try to get shrm of peers . how about that ??

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
