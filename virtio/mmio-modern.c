#include "kvm/virtio.h"
#include "kvm/virtio-mmio.h"

#include <linux/byteorder.h>

#define vmmio_selected_vq(vmmio) \
	vdev->ops->get_vq((vmmio)->kvm, (vmmio)->dev, (vmmio)->hdr.queue_sel)

#define ENABLE_MMIO_MODERN

#ifdef ENABLE_MMIO_MODERN
static void virtio_mmio_config_in(struct kvm_cpu *vcpu,
				  u64 addr, u32 *data, u32 len,
				  struct virtio_device *vdev, bool p9req)
{
	struct virtio_mmio *vmmio = vdev->virtio;
	u64 features = 1ULL << VIRTIO_F_VERSION_1;
	u32 val = 0;

	switch (addr) {
	case VIRTIO_MMIO_MAGIC_VALUE:
	case VIRTIO_MMIO_VERSION:
	case VIRTIO_MMIO_DEVICE_ID:
	case VIRTIO_MMIO_VENDOR_ID:
	case VIRTIO_MMIO_STATUS:
	case VIRTIO_MMIO_INTERRUPT_STATUS:
		val = *(u32 *)(((void *)&vmmio->hdr) + addr);
		break;
	case VIRTIO_MMIO_DEVICE_FEATURES:
		if (vmmio->hdr.host_features_sel > 1)
			break;
		features |= virtio_dev_get_host_features(vdev, vmmio->kvm, vmmio->dev);
		val = features >> (32 * vmmio->hdr.host_features_sel);
		break;
	case VIRTIO_MMIO_QUEUE_NUM_MAX:
		val = vdev->ops->get_size_vq(vmmio->kvm, vmmio->dev,
					     vmmio->hdr.queue_sel);
		break;
	case VIRTIO_MMIO_QUEUE_READY:
		val = vmmio_selected_vq(vmmio)->enabled;
		break;
	case VIRTIO_MMIO_QUEUE_DESC_LOW:
		val = vmmio_selected_vq(vmmio)->vring_addr.desc_lo;
		break;
	case VIRTIO_MMIO_QUEUE_DESC_HIGH:
		val = vmmio_selected_vq(vmmio)->vring_addr.desc_hi;
		break;
	case VIRTIO_MMIO_QUEUE_USED_LOW:
		val = vmmio_selected_vq(vmmio)->vring_addr.used_lo;
		break;
	case VIRTIO_MMIO_QUEUE_USED_HIGH:
		val = vmmio_selected_vq(vmmio)->vring_addr.used_hi;
		break;
	case VIRTIO_MMIO_QUEUE_AVAIL_LOW:
		val = vmmio_selected_vq(vmmio)->vring_addr.avail_lo;
		break;
	case VIRTIO_MMIO_QUEUE_AVAIL_HIGH:
		val = vmmio_selected_vq(vmmio)->vring_addr.avail_hi;
		break;
	case VIRTIO_MMIO_CONFIG_GENERATION:
		/*
		 * The config generation changes when the device updates a
		 * config field larger than 32 bits, that the driver reads using
		 * multiple accesses. Since kvmtool doesn't use any mutable
		 * config field larger than 32 bits, the generation is constant.
		 */
		break;
	default:
		return;
	}

	*data = cpu_to_le32(val);
}

static void virtio_mmio_config_out(struct kvm_cpu *vcpu,
				   u64 addr, u32 *data, u32 len,
				   struct virtio_device *vdev, bool p9req)
{
	struct virtio_mmio *vmmio = vdev->virtio;
	struct kvm *kvm = vmmio->kvm;
	u32 val = 0;
	u64 features;

	val = le32_to_cpu(*data);

	switch (addr) {
	case VIRTIO_MMIO_DEVICE_FEATURES_SEL:
	case VIRTIO_MMIO_DRIVER_FEATURES_SEL:
	case VIRTIO_MMIO_QUEUE_SEL:
		*(u32 *)(((void *)&vmmio->hdr) + addr) = val;
		break;
	case VIRTIO_MMIO_STATUS:
		vmmio->hdr.status = val;
		virtio_notify_status(kvm, vdev, vmmio->dev, val);
		break;
	case VIRTIO_MMIO_DRIVER_FEATURES:
		if (vmmio->hdr.guest_features_sel > 1)
			break;

		features = (u64)val << (32 * vmmio->hdr.guest_features_sel);
		virtio_set_guest_features(vmmio->kvm, vdev, vmmio->dev,
					  features);
		break;
	case VIRTIO_MMIO_QUEUE_NUM:
		vmmio->hdr.queue_num = val;
		vdev->ops->set_size_vq(vmmio->kvm, vmmio->dev,
				       vmmio->hdr.queue_sel, val);
		break;
	case VIRTIO_MMIO_QUEUE_READY:
		if (val)
			virtio_mmio_init_vq(kvm, vdev, vmmio->hdr.queue_sel);
		else
			virtio_mmio_exit_vq(kvm, vdev, vmmio->hdr.queue_sel);
		break;
	case VIRTIO_MMIO_QUEUE_NOTIFY:
		vdev->ops->notify_vq(vmmio->kvm, vmmio->dev, val);
		break;
	case VIRTIO_MMIO_INTERRUPT_ACK:
		vmmio->hdr.interrupt_state &= ~val;
		break;
	case VIRTIO_MMIO_QUEUE_DESC_LOW:
		vmmio_selected_vq(vmmio)->vring_addr.desc_lo = val;
		break;
	case VIRTIO_MMIO_QUEUE_DESC_HIGH:
		vmmio_selected_vq(vmmio)->vring_addr.desc_hi = val;
		break;
	case VIRTIO_MMIO_QUEUE_USED_LOW:
		vmmio_selected_vq(vmmio)->vring_addr.used_lo = val;
		break;
	case VIRTIO_MMIO_QUEUE_USED_HIGH:
		vmmio_selected_vq(vmmio)->vring_addr.used_hi = val;
		break;
	case VIRTIO_MMIO_QUEUE_AVAIL_LOW:
		vmmio_selected_vq(vmmio)->vring_addr.avail_lo = val;
		break;
	case VIRTIO_MMIO_QUEUE_AVAIL_HIGH:
		vmmio_selected_vq(vmmio)->vring_addr.avail_hi = val;
		break;
	};
}
#endif

extern struct virtio_ops p9_dev_virtio_ops;

void virtio_mmio_modern_callback(struct kvm_cpu *vcpu, u64 addr, u8 *data,
				 u32 len, u8 is_write, void *ptr)
{
#ifdef ENABLE_MMIO_MODERN
	struct virtio_device *vdev = ptr;
	struct virtio_mmio *vmmio = vdev->virtio;
	u32 offset = addr - vmmio->addr;
	bool p9req = false;

    // [JB] no_shared_region check
    // no_shared_region is dealt with in p9-backend
    if ( (vcpu->kvm->cfg.arch.realm_pv != NULL) && (strcmp(vcpu->kvm->cfg.arch.realm_pv, "no_shared_region") == 0) ) {
		// check if this is 9p device
		// if 9p --> keep going ahead, otherwise --> return;
		if ((unsigned long)(vdev->ops) == (unsigned long)(&p9_dev_virtio_ops)) {
			p9req = true;
		} else {
        	return;
		}
    }

	if (offset >= VIRTIO_MMIO_CONFIG) {
		offset -= VIRTIO_MMIO_CONFIG;
		virtio_access_config(vmmio->kvm, vdev, vmmio->dev, offset, data,
				     len, is_write);
		return;
	}

	if (len != 4) {
		pr_debug("Invalid %s size %d at 0x%llx", is_write ? "write" :
			 "read", len, addr);
		if (p9req)
			printf("[JB] p9: invalid size!\n");
		return;
	}

	if (is_write) {
		virtio_mmio_config_out(vcpu, offset, (void *)data, len, ptr, p9req);
	}
	else {
		virtio_mmio_config_in(vcpu, offset, (void *)data, len, ptr, p9req);
	}
#endif
}
