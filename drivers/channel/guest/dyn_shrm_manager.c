#include "dyn_shrm_manager.h"
#include "virt_pci_driver.h"
#include <linux/slab.h>

void set_memory_shared(phys_addr_t start, int numpages);

s64 get_shrm_chunk(void) {
	s64 shrm_ipa, mmio_ret;
	u32 shrm_id;

	pr_info("[GCH] %s read a new shrm_ipa using mmio trap", __func__);

	mmio_ret = mmio_read_to_get_shrm(SHRM_RW);
	shrm_ipa = mmio_ret & ~SHRM_ID_MASK;
	shrm_id = mmio_ret & SHRM_ID_MASK;
	if (!shrm_ipa || !shrm_id) {
		pr_err("[GCH] %s failed to get shrm_ipa. mmio_ret: %#llx", __func__, mmio_ret);
		return -EAGAIN;
	}

	pr_info("[GCH] %s get shrm_ipa 0x%llx from kvmtool", __func__, shrm_ipa);

	return mmio_ret;
}

s64 _req_shrm_chunk(bool was_requested) {
	s64 ret = 0;

	if (was_requested) {
		ret = get_shrm_chunk();
		return ret;
	}

	pr_info("%s: send_signal to the SHRM_ALLOCATOR", __func__);
	send_signal(SHRM_ALLOCATOR);
	return -EAGAIN;
}


#ifndef CONFIG_GUEST_CHANNEL_IO_RING
struct gen_pool* init_shrm_pool(void) {
	int cnt = 0, ret;
	struct packet_pos *cur_pp;
    struct gen_pool* shrm_pool = gen_pool_create(PAGE_SHIFT, -1);
	if (!shrm_pool) {
		pr_err("[GCH] %s gen_pool_create() failed\n", __func__);
		return NULL;
	}

	return shrm_pool;
}

int add_shrm_pool(struct gen_pool* shrm_pool, u64 va, u64 ipa, u64 size) {
	int ret;

	if (!shrm_pool) {
		pr_info("%s: shrm_pool shouldn't be NULL", __func__);
		return -EINVAL;
	}

	 ret = gen_pool_add_virt(shrm_pool, va, (phys_addr_t) ipa, (size_t)size, -1);
	 if (ret) {
		 pr_err("%s: gen_pool_add_virt() failed %d", __func__, ret);
	 }
	 pr_info("%s: va %#llx, ipa %#llx, size %#llx ret %d", __func__, va, ipa, size, ret);

	 return ret;
}
#endif


void print_front_rear(struct packet_pos* pp) {
	if (!pp) return;

	if (pp->front.shrm)
		pr_info("%s: pp->front.shrm->ipa %#llx, pp->front.shrm->shrm_id %d",
				__func__, pp->front.shrm->ipa, pp->front.shrm->shrm_id);

	if (pp->rear.shrm)
		pr_info("%s: pp->rear.shrm->ipa %#llx, pp->rear.shrm->shrm_id %d",
				__func__, pp->rear.shrm->ipa, pp->rear.shrm->shrm_id);
}

struct shrm_list* init_shrm_list(struct rings_to_send* rts, u64 ipa_start, u64 ipa_size) {
	int cnt = 0, ret;
	struct packet_pos *cur_pp;
    struct shrm_list* rw_shrms = kzalloc(sizeof(struct shrm_list), GFP_KERNEL);
	if (!rw_shrms) {
		pr_err("[GCH] %s failed to kzalloc for struct shrm_list\n", __func__);
		return NULL;
	}

	INIT_LIST_HEAD(&rw_shrms->head);
	rw_shrms->ipa_start = ipa_start;
	rw_shrms->ipa_end = ipa_start + ipa_size;

#ifndef CONFIG_GUEST_CHANNEL_IO_RING
	rw_shrms->shrm_pool = init_shrm_pool();
	if (!rw_shrms->shrm_pool) {
		return NULL;
	}
#endif

	do {
		if (cnt > 10) {
			pr_err("%s: req_shrm_chunk cnt %d. something wrong", __func__, cnt);
			return NULL;
		}
		ret = req_shrm_chunk(rts, rw_shrms);
		cnt++;
	} while(ret == -EAGAIN);

	cur_pp = &rw_shrms->pp;
	if (!cur_pp->rear.shrm) {
		cur_pp->front.shrm = list_first_entry(&rw_shrms->head, struct shared_realm_memory, head);
		cur_pp->rear.shrm = cur_pp->front.shrm;
		pr_info("%s: set cur_pp->front, rear: shrm ipa %#llx shrm_id %d",
				__func__, cur_pp->front.shrm->ipa, cur_pp->front.shrm->shrm_id);
	}

	return rw_shrms;
}

static bool is_valid_ipa(struct shrm_list* rw_shrms, u64 ipa_start, u64 ipa_size) {
	u64 ipa_end = ipa_start + ipa_size;
	return (rw_shrms->ipa_start <= ipa_start && ipa_end <= rw_shrms->ipa_end);
}

int remove_shrm_chunk(struct shrm_list* rw_shrms, u64 ipa) {
	struct shared_realm_memory *tmp, *target = NULL;

	pr_info("%s start", __func__);

	list_for_each_entry(tmp, &rw_shrms->head, head) {
		if (ipa == tmp->ipa) {
			target = tmp;

			if (target->in_use) {
				pr_err("%s: target shrm %#llx is in use", __func__, target->ipa);
				return -EINVAL;
			}
			list_del(&target->head);
			break;
		}
	}

	if (target) {
		pr_info("%s: remove target shrm. ipa: 0x%llx", __func__, ipa);
		kfree(target);
		mmio_write_to_remove_shrm(ipa);
		pr_info("%s end", __func__);
		return 0;
	}

	rw_shrms->free_size -= SHRM_CHUNK_SIZE;
	rw_shrms->total_size -= SHRM_CHUNK_SIZE;

	pr_err("%s there is no entry matched with the ipa 0x%llx", __func__, ipa);
	return -EINVAL;
}



static struct shared_realm_memory* get_new_shrm(s64 shrm_ipa, u32 shrm_id, SHRM_TYPE type) {
	struct shared_realm_memory *new_shrm;

		new_shrm = kzalloc(sizeof(*new_shrm), GFP_KERNEL);
		if (!new_shrm) {
			pr_err("[GCH] %s failed to kzalloc for new_shrm", __func__);
			return NULL;
		}

		new_shrm->ipa = shrm_ipa;
		new_shrm->shrm_id = shrm_id;
		new_shrm->type= type;

		return new_shrm;
}

int add_rw_shrm_chunk(struct rings_to_send* rts, struct shrm_list* rw_shrms, s64 shrm_ipa, u32 shrm_id) {
	struct shared_realm_memory *new_shrm;
	u64 va;
#ifdef CONFIG_GUEST_CHANNEL_IO_RING
	int desc_idx = -1, ret;
#endif

	if (!rw_shrms) {
		pr_err("[GCH] %s: rw_shrms shouldn't be NULL\n", __func__);
		return -1;
	}

	if (shrm_ipa < rw_shrms->ipa_start && rw_shrms->ipa_end <= shrm_ipa) {
		pr_err("[GCH] %s: invalid ipa %#llx", __func__, shrm_ipa);
		return -1;
	}

	new_shrm = get_new_shrm(shrm_ipa, shrm_id, SHRM_RW);
	if (!new_shrm) {
		pr_err("[GCH] %s failed to kzalloc for new_shrm", __func__);
		return -2;
	}

	list_add(&new_shrm->head, rw_shrms->head.prev);
	pr_info("%s: print front & rear", __func__);
	print_front_rear(&rw_shrms->pp);

	rw_shrms->free_size += SHRM_CHUNK_SIZE;
	rw_shrms->total_size += SHRM_CHUNK_SIZE;

#ifdef CONFIG_GUEST_CHANNEL_IO_RING
	desc_idx = desc_push_back(rts, 0, 0, IO_RING_DESC_F_DYN_ALLOC, shrm_id);
	if (desc_idx < 0) {
		pr_err("%s: desc_push_back failed %d", __func__, desc_idx);
	}

	ret = avail_push_back(rts, desc_idx);
	if (ret) {
		pr_err("avail_push_back() is failed %d", ret);
		return ret;
	}
#else
	va = (u64) get_shrm_va(SHRM_RW, new_shrm->ipa);
	return add_shrm_pool(rw_shrms->shrm_pool, va, new_shrm->ipa, SHRM_CHUNK_SIZE);
#endif
	return 0;
}



//TODO: need to be static. for now, it's opened just for the test
s64 req_shrm_chunk(struct rings_to_send* rts, struct shrm_list* rw_shrms) {
	s64 ret = 0;

	if (!rw_shrms) {
		pr_info("[GCH] %s rw_shrms shouldn't be NULL", __func__);
		return -EINVAL;
	}

	ret = _req_shrm_chunk(rw_shrms->add_req_pending);

	if (rw_shrms->add_req_pending) {
		s64 shrm_ipa;
		u32 shrm_id;

		shrm_ipa = ret & ~SHRM_ID_MASK;
		shrm_id = ret & SHRM_ID_MASK;

		if (!is_valid_ipa(rw_shrms, shrm_ipa, SHRM_CHUNK_SIZE)) {
			pr_err("[GCH] %s shrm_ipa 0x%llx is not valid", __func__, shrm_ipa);
			return -EINVAL;
		}

		pr_info("[GCH] %s call set_memory_shared with shrm_ipa 0x%llx", __func__, shrm_ipa);
		set_memory_shared(shrm_ipa, SHRM_CHUNK_SIZE / PAGE_SIZE);

		ret = add_rw_shrm_chunk(rts, rw_shrms, shrm_ipa, shrm_id);
		if (ret) 
			return ret;

		rw_shrms->add_req_pending = false;

		return ret;
	} else {
		rw_shrms->add_req_pending = true;
	}

	return ret;
}

bool invalid_packet_pos(struct packet_pos* pp) {
	if (!pp) {
		pr_err("%s: packet_pos shouldn't be NULL", __func__);
		return true;
	}

	if (!pp->front.shrm || !pp->rear.shrm) {
		pr_err("%s shrm shouldn't be NULL 0x%llx 0x%llx",
				__func__, pp->front.shrm, pp->rear.shrm);
		return true;
	}

	if (pp->front.shrm == pp->rear.shrm && pp->front.offset >= pp->rear.offset) {
		pr_err("%s front offset 0x%llx shouldn't bigger than rear offset 0x%llx in the same shrm",
				__func__, pp->front.offset, pp->rear.offset);
		return true;
	}

	return false;
}

#ifdef CONFIG_GUEST_CHANNEL_IO_RING
static int _write_to_shrm(struct shrm_list* rw_shrms, u64* va, const u64* data, u64 size) {
	if (rw_shrms->free_size < size) {
		pr_err("%s: not enough shrm. free_size: %#llx < size: %#llx",
				__func__, rw_shrms->free_size, size);
		return -1;
	}
	memcpy(va, data, size);
	rw_shrms->free_size -= size;
	return 0;
}

// Returns number of bytes that could not be written. On success, this will be zero.
int write_to_shrm(struct rings_to_send* rts, struct shrm_list* rw_shrms, struct packet_pos* pp, const void* data, u64 size) {
	struct packet_pos *cur_pp;
	struct shared_realm_memory *next_rear_shrm;
	int move_cnt = 0, ret;
	u64 written_size = 0, *dest_va, data_size = size;

	pr_info("[GCH] %s start. size %#llx", __func__, size);

	if (!rw_shrms || list_empty(&rw_shrms->head)) {
		pr_info("[GCH] %s rw_shrms or first list shouldn't be NULL. rw_shrms: %#llx",
				__func__, rw_shrms);
		return -EINVAL;
	}

	cur_pp = &rw_shrms->pp;
	next_rear_shrm = cur_pp->rear.shrm;

	if (!data) {
		pr_err("%s: data shouldn't be NULL", __func__, (u64)data);
		return -EINVAL;
	}

	if (!pp) {
		pr_err("%s: packet_pos shouldn't be NULL", __func__);
		return -EINVAL;
	}

	if (rw_shrms->free_size < MIN_FREE_SHRM_SIZE) {
		req_shrm_chunk(rts, rw_shrms);
	}

	if (rw_shrms->free_size < size || 
			rw_shrms->free_size - size < SHRM_CHUNK_SIZE) {
		req_shrm_chunk(rts, rw_shrms);
		return -EAGAIN;
	}

	pp->size = data_size;

	move_cnt = size / SHRM_CHUNK_SIZE;
	size %= SHRM_CHUNK_SIZE;

	if (size > (SHRM_CHUNK_SIZE - cur_pp->rear.offset)) {
		move_cnt++;
		size -= (SHRM_CHUNK_SIZE - cur_pp->rear.offset);
	}

	// data writing starts
	dest_va = get_shrm_va(SHRM_RW, next_rear_shrm->ipa + cur_pp->rear.offset);
	if (!dest_va) {
		pr_err("%s dest_va shouldn't be NULL", __func__);
		return -EINVAL;
	}

	if (size <= SHRM_CHUNK_SIZE - cur_pp->rear.offset) {
		ret = _write_to_shrm(rw_shrms, dest_va, data, data_size);
		if (ret)
			return ret;

		pr_info("%s: data 1: ", __func__);
		for (int i = 0; i * sizeof(data) < data_size; i++) {
			pr_info("%#llx i:%d ", dest_va[i], i);
		}

		pp->front = cur_pp->rear;
		cur_pp->rear.offset += data_size;
		cur_pp->size += data_size;
		pp->rear = cur_pp->rear;

		next_rear_shrm->in_use = true;
		pr_info("%s: print front & rear", __func__);
		print_front_rear(&rw_shrms->pp);

		return 0;
	}
	ret = _write_to_shrm(rw_shrms, dest_va, data, SHRM_CHUNK_SIZE - cur_pp->rear.offset);
	if (ret)
		return ret;

	pr_info("%s: data 2: ", __func__);
	for (u64 i = 0; i < SHRM_CHUNK_SIZE - cur_pp->rear.offset; i+= sizeof(u64)) {
		pr_cont("%llx", dest_va[i]);
	}

	written_size += SHRM_CHUNK_SIZE - cur_pp->rear.offset;
	next_rear_shrm->in_use = true;
	next_rear_shrm = list_next_entry(next_rear_shrm, head);

	for(int i = 1; i < move_cnt && data_size > written_size;
			i++, next_rear_shrm = list_next_entry(next_rear_shrm, head)) {
		dest_va = get_shrm_va(SHRM_RW, next_rear_shrm->ipa);

		ret = _write_to_shrm(rw_shrms, dest_va, data + written_size, SHRM_CHUNK_SIZE);
		if (ret)
			return ret;

		pr_info("%s: data 3: ", __func__);
		for (u64 i = 0; i < SHRM_CHUNK_SIZE; i+= sizeof(u64)) {
			pr_cont("%llx", dest_va[i]);
		}

		written_size += SHRM_CHUNK_SIZE;
		next_rear_shrm->in_use = true;
	}

	if (data_size < written_size) {
		pr_err("%s written size 0x%llx is bigger than data_size %0x%llx ",
				__func__, written_size, data_size);
		return -EINVAL;
	}

	dest_va = get_shrm_va(SHRM_RW, next_rear_shrm->ipa);
	ret = _write_to_shrm(rw_shrms, dest_va, data + written_size, data_size - written_size);
	if (ret)
		return ret;

	pr_info("%s: data 4: ", __func__);
	for (u64 i = 0; i < data_size - written_size; i+= sizeof(u64)) {
		pr_cont("%llx", dest_va[i]);
	}
	written_size += data_size - written_size;
	next_rear_shrm->in_use = true;

	pp->front = cur_pp->rear;
	cur_pp->rear.shrm = next_rear_shrm;
	cur_pp->rear.offset = data_size - written_size;
	cur_pp->size += data_size;

	pp->rear = cur_pp->rear;

	return data_size - written_size; // in normal case, should be zero
}
#else
int write_to_shrm(struct rings_to_send* rts, struct shrm_list* rw_shrms, struct packet_pos* pp, const void* data, u64 size) {
	return NULL;
}
#endif

// Returns number of bytes that could not be copied. On success, this will be zero.
int copy_from_shrm(void* to, struct packet_pos* from) {
	struct shared_realm_memory* cur_shrm;
	u64 written_size = 0, *src_va;

	if (!to) {
		pr_err("%s to pointer shouldn't be NULL", __func__);
		return -EINVAL;
	}

	if (invalid_packet_pos(from)) {
		pr_err("%s: packet_pos is invalid", __func__);
		return -EINVAL;
	}

	cur_shrm = from->front.shrm;

	src_va = get_shrm_va(SHRM_RO, cur_shrm->ipa + from->front.offset);
	if (!src_va) {
		pr_err("%s src_va shouldn't be NULL", __func__);
		return -EINVAL;
	}

	if (from->front.shrm == from->rear.shrm) {
		memcpy((void*)to, src_va, from->rear.offset - from->front.offset);
		written_size += from->rear.offset - from->front.offset;
		return from->size - written_size;
	}

	// copy from the shrms with va offset
	memcpy((void*)to, src_va, SHRM_CHUNK_SIZE - from->front.offset);
	written_size += SHRM_CHUNK_SIZE - from->front.offset;
	cur_shrm = list_next_entry(cur_shrm, head);

	for (; cur_shrm != from->rear.shrm && from->size > written_size;
			cur_shrm = list_next_entry(cur_shrm, head)) {
		if (!cur_shrm) {
			pr_err("%s cur_shrm shouldn't be NULL", __func__);
			return -EINVAL;
		}
		src_va = get_shrm_va(SHRM_RO, cur_shrm->ipa);
		memcpy((void*)to + written_size, src_va, SHRM_CHUNK_SIZE);
		written_size += SHRM_CHUNK_SIZE;
	}

	if (from->size < written_size) {
		pr_err("%s written size 0x%llx is bigger than from->size %0x%llx ",
				__func__, written_size, from->size);
		return -EINVAL;
	}

	src_va = get_shrm_va(SHRM_RO, cur_shrm->ipa);
	memcpy((void*)to + written_size, src_va, from->size - written_size);
	written_size += from->size - written_size;

	return (int)from->size - written_size;
}

struct shared_realm_memory* get_shrm_with(struct shrm_list* rw_shrms, u32 shrm_id) {
	struct shared_realm_memory* tmp;

	if (!rw_shrms) {
		pr_err("%s: input pointer shouldn't be NULL rw_shrms: %#llx", __func__, rw_shrms);
		return NULL;
	}

	list_for_each_entry(tmp, &rw_shrms->head, head) {
		if (shrm_id == tmp->shrm_id) {
			return tmp;
		}
	}

	return NULL;
}

static int delete_from_shrm(struct shrm_list* rw_shrms, u64* va, u64 size) {
	if (rw_shrms->free_size < size) {
		pr_err("%s: not enough shrm. free_size: %#llx < size: %#llx",
				__func__, rw_shrms->free_size, size);
		return -1;
	}
	memset(va, 0, size);
	rw_shrms->free_size += size;
	return 0;
}

//TODO: how about memset only one desc ? and just use it repeatedly
int delete_packet_from_shrm(struct packet_pos* pp, struct shrm_list* rw_shrms) {
	int ret;
	struct shared_realm_memory* cur;
	u64 *dest_va;

	pr_info("%s start", __func__);

	if (!pp || !rw_shrms) {
		pr_err("%s: input pointers shouldn't be NULL. pp: %#llx, rw_shrms: %#llx",
				__func__, pp, rw_shrms);
		return -1;
	}

	if (invalid_packet_pos(pp)) {
		pr_err("%s: packet_pos is invalid", __func__);
		return -2;
	}

	if (rw_shrms->pp.front.shrm != pp->front.shrm) {
		pr_err("%s: pp->front.shrm is not matched. rw_shrms->pp.front.shrm %#llx, pp->front.shrm: %#llx",
				__func__, rw_shrms->pp.front.shrm, pp->front.shrm);
		return -3;
	}

	if (rw_shrms->pp.front.offset != pp->front.offset) {
		pr_err("%s: pp->front.offset is not matched. rw_shrms->pp.front.offset %#llx, pp->front.offset: %#llx",
				__func__, rw_shrms->pp.front.offset, pp->front.offset);
		return -4;
	}

	if (pp->front.shrm == pp->rear.shrm) {
		dest_va = get_shrm_va(SHRM_RW, pp->front.shrm->ipa + pp->front.offset);
		pr_info("%s: memset va: %#llx, size: %#llx", __func__, dest_va, pp->rear.offset);

		ret = delete_from_shrm(rw_shrms, dest_va, pp->rear.offset);
		if (ret)
			return ret;

		rw_shrms->pp.front.offset = pp->rear.offset;
		pr_info("%s done 1", __func__);
		return 0;
	}

	cur = pp->front.shrm;
	for(;cur != pp->rear.shrm; cur = list_next_entry(cur, head)) {
		dest_va = get_shrm_va(SHRM_RW, cur->ipa);

		pr_info("%s: memset va: %#llx, size: %#llx", __func__, dest_va, SHRM_CHUNK_SIZE);
		ret = delete_from_shrm(rw_shrms, dest_va, SHRM_CHUNK_SIZE);
		if (ret)
			return ret;
	}

	dest_va = get_shrm_va(SHRM_RW, pp->rear.shrm->ipa);
	pr_info("%s: memset va: %#llx, size: %#llx", __func__, dest_va, pp->rear.offset);
	ret = delete_from_shrm(rw_shrms, dest_va, pp->rear.offset);
	if (ret)
		return ret;

	rw_shrms->pp.front.offset = pp->rear.offset;
	pr_info("%s done 2", __func__);
	return 0;
}

/***************************************************************************
 *
 * APIs for Read-Only shared realm memory chunks
 *
 * ************************************************************************/

int add_ro_shrm_chunk(struct list_head* ro_shrms_head, u32 shrm_id) {
	struct shared_realm_memory *new_shrm;
	u64 shrm_ipa = 0;

	shrm_ipa = req_ro_shrm_ipa(shrm_id);
	if (!shrm_ipa) {
		pr_err("%s: req_ro_shrm_ipa failed", __func__);
		return -1;
	}

	if (shrm_ipa < SHRM_RO_IPA_REGION_START && SHRM_RO_IPA_REGION_END <= shrm_ipa) {
		pr_err("[GCH] %s: %#llx is not within SHRM_RO_IPA_REGION range", __func__, shrm_ipa);
		return -2;
	}

	new_shrm = get_new_shrm(shrm_ipa, shrm_id, SHRM_RO);
	if (!new_shrm) {
		pr_err("[GCH] %s failed to kzalloc for new_shrm", __func__);
		return -3;
	}

	list_add(&new_shrm->head, ro_shrms_head);

	return 0;
}

u64 req_ro_shrm_ipa(u32 shrm_id) {
	u64 shrm_ro_ipa, returned_shrm_id, mmio_ret;

	mmio_write_to_get_ro_shrm(shrm_id);
	//TODO: use msi to use another irq for getting allocation of shrm
	mmio_ret = mmio_read_to_get_shrm(SHRM_RO);
	if (!mmio_ret) {
		pr_err("[GCH] %s: failed to get shrm_ro_ipa", __func__);
		return 0;
	}

	shrm_ro_ipa = mmio_ret & ~SHRM_ID_MASK;
	returned_shrm_id = mmio_ret & SHRM_ID_MASK;

	if (returned_shrm_id != shrm_id) {
		pr_err("%s: invalid shrm_id is returned. %d != %d",returned_shrm_id, shrm_id);
		return 0;
	}

	return shrm_ro_ipa;
}
