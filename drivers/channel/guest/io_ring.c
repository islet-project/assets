#include <linux/types.h>
#include <linux/limits.h>
#include <linux/align.h>
#include <linux/printk.h>
#include <asm/string.h>
#include "dyn_shrm_manager.h"
#include "io_ring.h"

struct desc_ring* create_desc_ring(u64* ipa_base) {
	struct desc_ring* desc_ring = (struct desc_ring*)ALIGN((u64)ipa_base, 8);

	if ((u64)ipa_base < SHRM_RW_IPA_REGION_START
			&& SHRM_RW_IPA_REGION_END <= (u64)ipa_base) {
		pr_err("%s invalid ipa_base %p", __func__, ipa_base);
		return NULL;
	}

	pr_info("%s desc_ring addr %p", __func__, desc_ring);

	return desc_ring;
}

static struct io_ring* init_io_ring(int noti_limit, u64 *ipa_base) {
	struct io_ring* io_ring = (struct io_ring *)ipa_base;

	memset(io_ring, 0, sizeof(*io_ring));
	io_ring->shrm_ipa_base = (u64)ipa_base;
	io_ring->noti_limit = noti_limit;

	return io_ring;
}

struct io_ring* avail_create(int noti_limit, u64* ipa_base) {
	struct io_ring* io_ring = NULL;

	if ((u64)ipa_base < SHRM_RW_IPA_REGION_START && SHRM_RW_IPA_REGION_END <= (u64)ipa_base) {
		pr_err("%s invalid ipa_base %p", __func__, ipa_base);
		return NULL;
	}

	io_ring = init_io_ring(noti_limit, ipa_base);
	pr_info("%s io_ring addr %p", __func__, io_ring);

	return io_ring;
}

struct io_ring* used_create(int noti_limit, u64* ipa_base) {
	struct io_ring* io_ring = NULL;

	if ((u64)ipa_base < SHRM_RO_IPA_REGION_START && SHRM_RO_IPA_REGION_END <= (u64)ipa_base) {
		pr_err("%s invalid ipa_base %p", __func__, ipa_base);
		return NULL;
	}

	io_ring = init_io_ring(noti_limit, ipa_base);
	pr_info("%s io_ring addr %p", __func__, io_ring);

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
void init_rings_to_send(struct rings_to_send* rts, u64 shrm_ipa) {
	rts->avail = avail_create(1, (u64*)shrm_ipa);
	shrm_ipa += sizeof(*rts->avail);

	rts->desc_ring = create_desc_ring((u64*)shrm_ipa);
}

//TODO: implement it!
/*
int io_ring_pop_front(struct io_ring* io_ring, u16 desc_idx) {
}
*/

int desc_push_back(struct rings_to_send* rings_to_send, u64 ipa, u32 len, u16 flags) {
	int idx;
	struct desc_ring* desc_ring = rings_to_send->desc_ring;

	if (!rings_to_send || !desc_ring)
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


