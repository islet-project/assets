#include <linux/types.h>
#include <linux/limits.h>
#include <linux/align.h>
#include <asm/string.h>
#include <asm-generic/barrier.h>
#include "dyn_shrm_manager.h"
#include "io_ring.h"

//TODO: setup rings_to_send & rings_to_receive

struct io_ring* avail_create(int noti_limit, u64* base_ipa) {
	struct io_ring* io_ring;

	if (base_ipa < SHRM_RW_IPA_START && SHRM_RW_IPA_END <= base_ipa) {
		pr_err("%s invalid base_ipa %p", __func__, base_ipa);
		return NULL;
	}

	init_io_ring(io_ring, noti_limit, base_ipa);
	pr_info("%s io_ring addr %p", __func__, io_ring);

	return io_ring;
}

struct io_ring* used_create(int noti_limit, u64* base_ipa) {
	struct io_ring* io_ring;

	if (base_ipa < SHRM_RO_IPA_START && SHRM_RO_IPA_END <= base_ipa) {
		pr_err("%s invalid base_ipa %p", __func__, base_ipa);
		return NULL;
	}

	init_io_ring(io_ring, noti_limit, base_ipa);
	pr_info("%s io_ring addr %p", __func__, io_ring);

	return io_ring;
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

//TODO: implement it!
/*
int io_ring_pop_front(struct io_ring* io_ring, u16 desc_idx) {
}
*/

static void init_io_ring(struct io_ring* io_ring, int noti_limit, u64 *base_ipa) {
	io_ring = base_ipa;

	memset(io_ring, 0, sizeof(io_ring));
	io_ring->base_ipa = (u64)base_ipa;
	io_ring->noti_cnt = noti_cnt;
}

int desc_push_back(struct rings_to_send* rings_to_send, u64 ipa, u32 len, u16 flags) {
	int idx;
	struct desc_ring* desc_ring = rings_to_send->desc_ring;

	if (!rings_to_send || !desc || !desc_ring)
		return -EINVAL;

	idx = desc_ring->rear;

	desc_ring->ring[idx].ipa = ipa;
	desc_ring->ring[idx].len = len;
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

struct desc_ring* create_desc_ring(u64* base_ipa, u64 offset) {
	struct desc_ring* desc_ring = align(base_ipa + offset, 8);

	pr_info("%s desc_ring addr %p", __func__, desc_ring);

	return desc_ring;
}
