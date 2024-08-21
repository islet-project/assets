#include "dyn_shrm_manager.h"
#include <linux/slab.h>

#define KVMTOOL_ID 0

//TODO: Client should manage the shrm_list. don't use it as global.
//      Create function to allocate it and every function should get shrm_list as argument
static struct shrm_list* rw_shrms;

void send_signal(int peer_id);
s64 mmio_read_to_get_shrm(void);
int mmio_write_to_remove_shrm(u64 ipa);
u64* get_shrm_va(bool read_only, u64 ipa);
void set_memory_shared(phys_addr_t start, int numpages);

int init_shrm_list(u64 ipa_start, u64 ipa_size) {
    rw_shrms = kzalloc(sizeof(struct shrm_list), GFP_KERNEL);
	if (!rw_shrms) {
		pr_err("[GCH] %s failed to kzalloc for struct shrm_list\n", __func__);
		return -ENOMEM;
	}

	INIT_LIST_HEAD(&rw_shrms->head);
	rw_shrms->ipa_start = ipa_start;
	rw_shrms->ipa_end = ipa_start + ipa_size;

	return 0;
}

bool is_valid_ipa(u64 ipa_start, u64 ipa_size) {
	u64 ipa_end = ipa_start + ipa_size;
	return (rw_shrms->ipa_start <= ipa_start && ipa_end <= rw_shrms->ipa_end);
}

int remove_shrm_chunk(u64 ipa) {
	struct shared_realm_memory *tmp, *target = NULL;

	pr_info("%s start", __func__);

	list_for_each_entry(tmp, &rw_shrms->head, head) {
		if (ipa == tmp->ipa) {
			target = tmp;
			list_del(&tmp->head);
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

	pr_err("%s there is no entry matched with the ipa 0x%llx", __func__, ipa);
	return -EINVAL;
}

static void set_req_pending(void) {
	if (!rw_shrms) {
		pr_info("[GCH] %s rw_shrms shouldn't be NULL", __func__);
		return;
	}
	if (rw_shrms->add_req_pending)
		return;
	rw_shrms->add_req_pending = true;
	send_signal(KVMTOOL_ID);
}

int add_shrm_chunk(s64 shrm_ipa) {
		struct shared_realm_memory *new_shrm;

		new_shrm = kzalloc(sizeof(*new_shrm), GFP_KERNEL);
		if (!new_shrm) {
			pr_err("[GCH] %s failed to kzalloc for new_shrm", __func__);
			return -ENOMEM;
		}

		new_shrm->ipa = shrm_ipa;
		if (!rw_shrms->pp.rear.shrm) {
			list_add(&new_shrm->head, &rw_shrms->head);
			rw_shrms->pp.front.shrm = new_shrm;
			rw_shrms->pp.rear.shrm = new_shrm;
		} else {
			list_add(&new_shrm->head, &rw_shrms->pp.rear.shrm->head);
		}
		return 0;
}

//TODO: need to be static. for now, it's opened just for the test
int req_shrm_chunk(void) {
	int ret = 0;

	if (!rw_shrms) {
		pr_info("[GCH] %s rw_shrms shouldn't be NULL", __func__);
		return -EINVAL;
	}

	if (rw_shrms->add_req_pending) {
		s64 shrm_ipa;

		pr_info("[GCH] %s read a new shrm_ipa using mmio trap", __func__);

		shrm_ipa = mmio_read_to_get_shrm();
		if (shrm_ipa <= 0) {
			pr_err("[GCH] %s failed to get shrm_ipa with %d", __func__, shrm_ipa);
			return -EAGAIN;
		}
		pr_info("[GCH] %s get shrm_ipa 0x%llx from kvmtool", __func__, shrm_ipa);

		if (!is_valid_ipa(shrm_ipa, SHRM_CHUNK_SIZE)) {
			pr_err("[GCH] %s shrm_ipa 0x%llx is not valid", __func__, shrm_ipa);
			return -EINVAL;
		}

		set_memory_shared(shrm_ipa, 1);
		pr_info("[GCH] %s call set_memory_shared with shrm_ipa 0x%llx", __func__, shrm_ipa);

		ret = add_shrm_chunk(shrm_ipa);
		if (ret) 
			return ret;

		rw_shrms->add_req_pending = false;
	} else {
		pr_info("[GCH] %s set_req_pending", __func__);
		set_req_pending();
		return -EAGAIN;
	}
	return 0;
}

bool invalid_packet_pos(struct packet_pos* pp) {
	if (!pp)
		return true;

	if (!pp->front.shrm || !pp->rear.shrm) {
		pr_err("%s shrm shouldn't be NULL 0x%llx 0x%llx",
				__func__, pp->front.shrm, pp->rear.shrm);
		return true;
	}

	if (pp->front.shrm == pp->rear.shrm && pp->front.offset >= pp->rear.offset) {
		pr_err("%s front offset 0x%llx shouldn't bigger than rear offset 0x%llx",
				__func__, pp->front.offset, pp->rear.offset);
		return true;
	}

	return false;
}

// Returns number of bytes that could not be written. On success, this will be zero.
int write_to_shrm(struct packet_pos* pp, const void* data, u64 size) {
	struct packet_pos *cur_pp;
	struct shared_realm_memory *next_rear_shrm;
	int move_cnt = 0;
	u64 written_size = 0, *dest_va, data_size = size;

	if (!rw_shrms) {
		pr_info("[GCH] %s rw_shrms shouldn't be NULL", __func__);
		return -EINVAL;
	}

	cur_pp = &rw_shrms->pp;
	next_rear_shrm = cur_pp->rear.shrm;

	if (!data) {
		pr_err("%s data shouldn't be NULL", __func__, (u64)data);
		return -EINVAL;
	}

	if (invalid_packet_pos(pp)) {
		pr_err("%s packet_pos is invalid", __func__);
		return -EINVAL;
	}

	if (rw_shrms->free_size < MIN_FREE_SHRM_SIZE) {
		req_shrm_chunk();
	}

	if (rw_shrms->free_size < size || 
			rw_shrms->free_size - size < SHRM_CHUNK_SIZE) {
		return req_shrm_chunk();
	}

	pp->size = data_size;

	move_cnt = size / SHRM_CHUNK_SIZE;
	size %= SHRM_CHUNK_SIZE;

	if (size > (SHRM_CHUNK_SIZE - cur_pp->rear.offset)) {
		move_cnt++;
		size -= (SHRM_CHUNK_SIZE - cur_pp->rear.offset);
	}

	// data writing starts
	dest_va = get_shrm_va(false, next_rear_shrm->ipa + cur_pp->rear.offset);
	if (!dest_va) {
		pr_err("%s dest_va shouldn't be NULL", __func__);
		return -EINVAL;
	}

	if (size <= SHRM_CHUNK_SIZE - cur_pp->rear.offset) {
		memcpy(dest_va, data, data_size);

		pp->front = cur_pp->rear;
		cur_pp->rear.offset += data_size;
		cur_pp->size += data_size;
		pp->rear = cur_pp->rear;

		return 0;
	}

	memcpy(dest_va, data, SHRM_CHUNK_SIZE - cur_pp->rear.offset);
	written_size += SHRM_CHUNK_SIZE - cur_pp->rear.offset;
	next_rear_shrm = list_next_entry(next_rear_shrm, head);

	for(int i = 0; i < move_cnt && data_size > written_size;
			i++, next_rear_shrm = list_next_entry(next_rear_shrm, head)) {
		dest_va = get_shrm_va(false, next_rear_shrm->ipa);
		memcpy(dest_va, data + written_size, SHRM_CHUNK_SIZE);
		written_size += SHRM_CHUNK_SIZE;
	}

	if (data_size < written_size) {
		pr_err("%s written size 0x%llx is bigger than data_size %0x%llx ",
				__func__, written_size, data_size);
		return -EINVAL;
	}

	dest_va = get_shrm_va(false, next_rear_shrm->ipa);
	memcpy(dest_va, data + written_size, data_size - written_size);
	written_size += data_size - written_size;

	pp->front = cur_pp->rear;
	cur_pp->rear.shrm = next_rear_shrm;
	cur_pp->rear.offset = data_size - written_size;
	cur_pp->size += data_size;

	pp->rear = cur_pp->rear;

	return data_size - written_size; // in normal case, should be zero
}

// Returns number of bytes that could not be copied. On success, this will be zero.
int copy_from_shrm(void* to, struct packet_pos* from) {
	struct shared_realm_memory* cur_shrm;
	u64 written_size = 0, *src_va;

	if (!to) {
		pr_err("%s to pointer shouldn't be NULL", __func__);
		return -EINVAL;
	}

	if (invalid_packet_pos(from)) {
		pr_err("%s packet_pos is invalid", __func__);
		return -EINVAL;
	}

	cur_shrm = from->front.shrm;

	src_va = get_shrm_va(true, cur_shrm->ipa + from->front.offset);
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
		src_va = get_shrm_va(true, cur_shrm->ipa);
		memcpy((void*)to + written_size, src_va, SHRM_CHUNK_SIZE);
		written_size += SHRM_CHUNK_SIZE;
	}

	if (from->size < written_size) {
		pr_err("%s written size 0x%llx is bigger than from->size %0x%llx ",
				__func__, written_size, from->size);
		return -EINVAL;
	}

	src_va = get_shrm_va(true, cur_shrm->ipa);
	memcpy((void*)to + written_size, src_va, from->size - written_size);
	written_size += from->size - written_size;

	return (int)from->size - written_size;
}
