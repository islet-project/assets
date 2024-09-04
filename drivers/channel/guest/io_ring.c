#include <linux/types.h>
#include <linux/limits.h>
#include <linux/align.h>
#include <linux/printk.h>
#include <asm/string.h>
#include "dyn_shrm_manager.h"
#include "io_ring.h"

struct desc_ring* create_desc_ring(u64 ipa_base, u64* va) {
	struct desc_ring* desc_ring = (struct desc_ring*)ALIGN((u64)va, 8);

	pr_info("%s start. desc_ring: %llx, va %llx, sizeof(*desc_ring) %llx",
			__func__, desc_ring, va, sizeof(*desc_ring));

	if (ipa_base < SHRM_RW_IPA_REGION_START && SHRM_RW_IPA_REGION_END <= ipa_base) {
		pr_err("%s invalid ipa_base %llx", __func__, ipa_base);
		return NULL;
	}

	pr_info("%s desc_ring addr %llx", __func__, desc_ring);

	return desc_ring;
}

static struct io_ring* init_io_ring(int noti_limit, u64 ipa_base, u64* va) {
	struct io_ring* io_ring = (struct io_ring *)va;
	pr_info("%s start. io_ring: %llx, va %llx, sizeof(*io_ring) %llx",
			__func__, io_ring, va, sizeof(*io_ring));

	memset(io_ring, 0, sizeof(*io_ring));
	io_ring->shrm_ipa_base = (u64)ipa_base;
	io_ring->noti_limit = noti_limit;

	pr_info("%s done", __func__);

	return io_ring;
}

struct io_ring* avail_create(int noti_limit, u64 ipa_base, u64* va) {
	struct io_ring* io_ring = NULL;
	pr_info("%s start", __func__);

	if (ipa_base < SHRM_RW_IPA_REGION_START && SHRM_RW_IPA_REGION_END <= ipa_base) {
		pr_err("%s invalid ipa_base %llx", __func__, ipa_base);
		return NULL;
	}

	io_ring = init_io_ring(noti_limit, ipa_base, va);
	pr_info("%s io_ring addr %llx", __func__, io_ring);

	return io_ring;
}

struct io_ring* used_create(int noti_limit, u64 ipa_base, u64* va) {
	struct io_ring* io_ring = NULL;

	if (ipa_base < SHRM_RW_IPA_REGION_START && SHRM_RW_IPA_REGION_END <= ipa_base) {
		pr_err("%s invalid ipa_base %llx", __func__, ipa_base);
		return NULL;
	}

	io_ring = init_io_ring(noti_limit, ipa_base, va);
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
	u64* shrm_rw_va = get_shrm_va(SHRM_RW, shrm_rw_ipa);
	pr_info("%s start", __func__);

	if (!rts || !rtr || !shrm_rw_va) {
		pr_err("%s: input pointers shouldn't be NULL. rts: %llx, rtr: %llx, shrm_rw_va: %llx",
				__func__, rts, rtr, shrm_rw_va);
		return -1;
	}
	rts->avail = avail_create(1, shrm_ipa + AVAIL_RING_OFFSET, shrm_rw_va + AVAIL_RING_OFFSET);
	if (!rts->avail) {
		pr_err("%s avail_create is failed", __func__);
		return -2;
	}

	rts->desc_ring = create_desc_ring(shrm_ipa + DESC_RING_OFFSET, shrm_rw_va + DESC_RING_OFFSET);
	if (!rts->desc_ring) {
		pr_err("%s create_desc_ring is failed", __func__);
		return -3;
	}

	rtr->used = used_create(1, shrm_ipa + USED_RING_OFFSET, shrm_rw_va + USED_RING_OFFSET);
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

	rtr->peer_avail = (struct io_ring*)shrm_ro_va + AVAIL_RING_OFFSET;
	rtr->peer_desc_ring = (struct io_ring*)shrm_ro_va + DESC_RING_OFFSET;
	rts->peer_used = (struct io_ring*)shrm_ro_va + USED_RING_OFFSET;

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


