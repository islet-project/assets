#include <linux/types.h>
#include <linux/limits.h>
#include <linux/align.h>
#include <linux/printk.h>
#include <linux/slab.h>
#include <asm/string.h>
#include "dyn_shrm_manager.h"
#include "io_ring.h"
#include "virt_pci_driver.h"

struct desc_ring* create_desc_ring(u64 ipa_base) {
	u64* va = get_shrm_va(SHRM_RW, ipa_base);
	struct desc_ring* desc_ring = (struct desc_ring*)ALIGN((u64)va, 8);

	pr_info("%s start. desc_ring: %llx, va %llx, sizeof(*desc_ring) %llx",
			__func__, desc_ring, va, sizeof(*desc_ring));

	if (!va) {
		pr_err("%s: failed to get shrm_va of ipa %llx", __func__, ipa_base);
		return NULL;
	}

	if (ipa_base < SHRM_RW_IPA_REGION_START && SHRM_RW_IPA_REGION_END <= ipa_base) {
		pr_err("%s invalid ipa_base %llx", __func__, ipa_base);
		return NULL;
	}

	memset(desc_ring, 0, sizeof(*desc_ring));

	pr_info("%s desc_ring addr %llx", __func__, desc_ring);

	return desc_ring;
}

static struct io_ring* init_io_ring(int noti_limit, u64 ipa_base) {
	u64* va = get_shrm_va(SHRM_RW, ipa_base);
	struct io_ring* io_ring = (struct io_ring *)va;

	if (!va) {
		pr_err("%s: failed to get shrm_va of ipa %llx", __func__, ipa_base);
		return NULL;
	}

	pr_info("%s start. io_ring: %llx, va %llx, sizeof(*io_ring) %llx",
			__func__, io_ring, va, sizeof(*io_ring));

	memset(io_ring, 0, sizeof(*io_ring));
	io_ring->shrm_ipa_base = (u64)ipa_base;
	io_ring->noti_limit = noti_limit;

	pr_info("%s done", __func__);

	return io_ring;
}

struct io_ring* avail_create(int noti_limit, u64 ipa_base) {
	struct io_ring* io_ring = NULL;
	pr_info("%s start", __func__);

	if (ipa_base < SHRM_RW_IPA_REGION_START && SHRM_RW_IPA_REGION_END <= ipa_base) {
		pr_err("%s invalid ipa_base %llx", __func__, ipa_base);
		return NULL;
	}

	io_ring = init_io_ring(noti_limit, ipa_base);
	pr_info("%s io_ring addr %llx", __func__, io_ring);

	return io_ring;
}

struct io_ring* used_create(int noti_limit, u64 ipa_base) {
	struct io_ring* io_ring = NULL;

	if (ipa_base < SHRM_RW_IPA_REGION_START && SHRM_RW_IPA_REGION_END <= ipa_base) {
		pr_err("%s invalid ipa_base %llx", __func__, ipa_base);
		return NULL;
	}

	io_ring = init_io_ring(noti_limit, ipa_base);
	pr_info("%s io_ring addr %llx", __func__, io_ring);

	return io_ring;
}

static int io_ring_push_back(struct io_ring* io_ring, u16 desc_idx) {
	if (!io_ring) 
		return -EINVAL;

	if (io_ring->rear + 1 == io_ring->front) 
		return -ENOSPC;

	io_ring->ring[io_ring->rear] = desc_idx;

	virt_wmb();

	io_ring->rear++;

	pr_info("%s: front: %d, rear: %d, desc_idx: %d, ring[rear-1]: %d",
			__func__, io_ring->front, io_ring->rear, desc_idx, io_ring->ring[io_ring->rear-1]);
	return 0;
}

int avail_push_back(struct rings_to_send* rings_to_send, u16 desc_idx) {
	if (!rings_to_send) 
		return -EINVAL;

	return io_ring_push_back(rings_to_send->avail, desc_idx);
}

int used_push_back(struct rings_to_receive* rings_to_recv, u16 desc_idx) {
	if (!rings_to_recv) 
		return -EINVAL;

	return io_ring_push_back(rings_to_recv->used, desc_idx);
}

//TODO: setup rings_to_send & rings_to_receive
int init_rw_rings(struct rings_to_send* rts, struct rings_to_receive* rtr, u64 shrm_ipa) {
	u64* shrm_rw_va = get_shrm_va(SHRM_RW, shrm_ipa);
	pr_info("%s start", __func__);

	if (!rts || !rtr || !shrm_rw_va) {
		pr_err("%s: input pointers shouldn't be NULL. rts: %llx, rtr: %llx, shrm_rw_va: %llx",
				__func__, rts, rtr, shrm_rw_va);
		return -1;
	}

	pr_info("%s: avail offset %llx, desc_ring offset %llx, used_ring offset %llx",
			__func__, AVAIL_RING_OFFSET, DESC_RING_OFFSET, USED_RING_OFFSET);
	rts->avail = avail_create(1, shrm_ipa + AVAIL_RING_OFFSET);
	if (!rts->avail) {
		pr_err("%s avail_create is failed", __func__);
		return -2;
	}

	rts->desc_ring = create_desc_ring(shrm_ipa + DESC_RING_OFFSET);
	if (!rts->desc_ring) {
		pr_err("%s create_desc_ring is failed", __func__);
		return -3;
	}

	rtr->used = used_create(1, shrm_ipa + USED_RING_OFFSET);
	if (!rtr->used) {
		pr_err("%s: used_create is failed", __func__);
		return -4;
	}

	return 0;
}

int init_ro_rings(struct rings_to_send* rts, struct rings_to_receive* rtr, u64 shrm_ro_ipa) {
	u64* shrm_ro_va = get_shrm_va(SHRM_RO, shrm_ro_ipa);
	pr_info("%s start", __func__);

	if (!rts || !rtr || !shrm_ro_va) {
		pr_err("%s: input pointers shouldn't be NULL. rts: %llx, rtr: %llx, shrm_ro_va: %llx",
				__func__, rts, rtr, shrm_ro_va);
		return -1;
	}

	rtr->peer_avail = (struct io_ring*)get_shrm_va(SHRM_RO, shrm_ro_ipa + AVAIL_RING_OFFSET);
	rtr->peer_desc_ring = (struct desc_ring*)get_shrm_va(SHRM_RO, shrm_ro_ipa + DESC_RING_OFFSET);
	rts->peer_used = (struct io_ring*)get_shrm_va(SHRM_RO, shrm_ro_ipa + USED_RING_OFFSET);

	pr_info("%s: peer_avail %llx, peer_desc_ring %llx, peer_used %llx",
			__func__, rtr->peer_avail, rtr->peer_desc_ring, rts->peer_used);

	return 0;
}

//TODO: implement it!
/*
int io_ring_pop_front(struct io_ring* io_ring, u16 desc_idx) {
}
*/

int desc_push_back(struct rings_to_send* rings_to_send, u64 offset, u32 len, u16 flags, u16 shrm_id) {
	int idx;
	struct desc_ring* desc_ring = rings_to_send->desc_ring;

	if (!rings_to_send || !desc_ring)
		return -EINVAL;

	idx = desc_ring->rear;

	desc_ring->ring[idx].offset = offset;
	desc_ring->ring[idx].len = len;
	desc_ring->ring[idx].shrm_id = shrm_id;
	desc_ring->ring[idx].flags = flags;

	desc_ring->rear++;

	pr_info("%s: offset: %#llx, len: %#llx, shrm_id: %d, flags %d",
			__func__, offset, len, shrm_id, flags);

	return idx;
}

/* temporarily comment it before using it
int desc_pop_front(struct rings_to_send* rings_to_send, int idx) {
	struct desc_ring* desc_ring = rings_to_send->desc_ring;

	if (!rings_to_send || !desc_ring)
		return -EINVAL;

	if (idx != desc_ring->front) {
		pr_err("%s not matched idx: %d with front %d",
				__func__, target_idx, desc_ring->front);
		return -EINVAL;
	}

	desc_ring[idx] = {};
	desc_ring->front = desc_ring->front + 1;

	return 0;
}
*/

int read_desc(struct desc* desc, struct list_head* ro_shrms_head) {
	struct shared_realm_memory *cur, *target = NULL;
	u64 *data, *ro_shrm_va;

	pr_info("%s start", __func__);

	if (!desc || !ro_shrms_head) {
		pr_err("%s: input ptrs shouldn't be null. desc %llx, ro_shrms_head %llx",
				__func__, desc, ro_shrms_head);
		return -1;
	}

	pr_info("%s: desc info: offset: %#llx, len: %#llx, shrm_id: %d, flags %d",
			__func__, desc->offset, desc->len, desc->shrm_id, desc->flags);

	data = kzalloc(desc->len, GFP_KERNEL);
	if (!data) {
		pr_err("[GCH] %s: failed to kzalloc", __func__);
		return -ENOMEM;
	}

	list_for_each_entry(cur, ro_shrms_head, head) {
		if (desc->shrm_id == cur->shrm_id) {
			target = cur;
			break;
		}
	}

	if (!target) {
		pr_err("%s: there is no proper shrm", __func__);
		return -2;
	}

	if (target->type != SHRM_RO) {
		pr_err("%s: target shrm type is not RO %llx", __func__, cur->type);
		return -3;
	}

	ro_shrm_va = get_shrm_va(SHRM_RO, target->ipa + desc->offset);
	if (!ro_shrm_va) {
		pr_err("%s ro_shrm_va shouldn't be NULL", __func__);
		return -4;
	}

	pr_info("%s: memcpy from ro_shrm", __func__);
	memcpy(data, ro_shrm_va, desc->len);

	pr_info("%s: start to print the data", __func__);
	for (u64 i = 0; i < desc->len; i++) {
		pr_cont("%llx", data[i]);
	}

	pr_info("%s done", __func__);
	return 0;
}
