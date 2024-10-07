#include "dyn_shrm_manager.h"
#include "shrm.h"
#include "channel.h"
#include <linux/printk.h>

void notify_peer(void);

/* 
 * It reads packets from peer_avail_ring and then writes the same descriptor index into used_ring
 * to notify that the descriptor entries are read 
 */
int read_packet(struct rings_to_receive* rtr, struct list_head* ro_shrms_head, u64* data) {
	int ret;
	struct io_ring* peer_avail;
	struct desc_ring* peer_desc_ring;
	u16 i;
	bool need_to_notify = false;
	u64 offset = 0;

	pr_info("%s start", __func__);

	pr_info("%s: peer_avail %llx, peer_desc_ring %llx",
			__func__, rtr->peer_avail, rtr->peer_desc_ring);

	if (!rtr || !ro_shrms_head || !data) {
		pr_err("%s: input pointers shouldn't be NULL. rtr: %llx, ro_shrms_head: %llx, data: %#llx",
				__func__, rtr, ro_shrms_head, data);
		return -1;
	}

	peer_avail = rtr->peer_avail;
	peer_desc_ring = rtr->peer_desc_ring;
	if (!peer_avail || !peer_desc_ring) {
		pr_err("%s: input pointers shouldn't be NULL. peer_avail: %llx, peer_desc_ring: %llx",
				__func__, peer_avail, peer_desc_ring);
		return -2;
	}

	i = peer_avail->front;
	pr_info("%s: peer_avail->front %d", __func__, i);
	if (MAX_DESC_RING <= i) {
		pr_err("%s: peer_avail->front %d shouldn't bigger than %d",
				__func__, i, MAX_DESC_RING);
		return -3;
	}

	while(i != peer_avail->rear) {
		u16 desc_idx = peer_avail->ring[i];
		struct desc* desc;
		pr_info("%s: desc_idx %d", __func__, desc_idx);

		do {
			desc = &peer_desc_ring->ring[desc_idx++];

			if (desc->flags & IO_RING_DESC_F_DYN_ALLOC) {
				pr_info("%s: IO_RING_DESC_F_DYN_ALLOC shrm_id %d", __func__, desc->shrm_id);
				ret = add_ro_shrm_chunk(ro_shrms_head, desc->shrm_id);
				if (ret) {
					pr_err("%s: add_ro_shrm_chunk failed %d", __func__, ret);
					return ret;
				}
			} else if (desc->flags & IO_RING_DESC_F_DYN_FREE) {
				// TODO: remove_ro_shrm_chunk();
			} else {
				u64 data_start = (u64)data;
				data_start += offset;
				pr_info("%s: data: %#llx, offset: %#llx, data_start: %#llx",
						__func__, (u64)data, offset, data_start);

				ret = read_desc(desc, ro_shrms_head, (u64*)data_start);
				if (ret < 0) {
					pr_err("%s: read_desc failed %d", __func__, ret);
					return ret;
				}
				offset += ret;
			}
		} while(desc->flags & IO_RING_DESC_F_NEXT);

		ret = used_push_back(rtr, peer_avail->ring[i]);
		if (ret) {
			pr_err("used_push_back() is failed %d", ret);
			return ret;
		}

		i = (i + 1) % MAX_DESC_RING;
		need_to_notify = true;
	}

	if (need_to_notify)
		notify_peer();
	pr_info("%s done", __func__);
	return 0;
}

int write_packet(struct rings_to_send* rts, struct shrm_list* rw_shrms, const void* data, u64 size) {
	int ret, first_desc_idx = -1;
	struct packet_pos pp = {};
	struct shared_realm_memory* cur_shrm = NULL;

	pr_info("%s start", __func__);

	if (!rts || !rw_shrms || !data) {
		pr_err("%s: input pointers shouldn't be NULL. rts: %llx, rw_shrms: %llx, data: %#llx",
				__func__, rts, rw_shrms, data);
		return -1;
	}

	do {
		ret = write_to_shrm(rts, rw_shrms, &pp, data, size);
	} while(ret == -EAGAIN);

	if (ret != 0) {
		pr_err("%s: write_to_shrm() is failed %d", __func__, ret);
		return ret;
	}
	pr_info("%s: write_to_shrm done", __func__);

	if (invalid_packet_pos(&pp)) {
		pr_err("%s: invalid packet_pos", __func__);
		return -EINVAL;
	}

	pr_info("%s: print front & rear", __func__);
	print_front_rear(&pp);

	for (cur_shrm = pp.front.shrm;; cur_shrm = list_next_entry(cur_shrm, head)) {
		u64 offset = 0;
		u16 flags = IO_RING_DESC_F_NEXT;

		pr_info("%s: cur_shrm ipa %#llx, shrm_id %d", __func__, cur_shrm->ipa, cur_shrm->shrm_id);

		if (pp.front.shrm == pp.rear.shrm) {
			offset = pp.front.offset;
			size = pp.rear.offset - pp.front.offset;
			flags = 0;
		} else if (cur_shrm == pp.front.shrm) {
			offset = pp.front.offset;
			size = SHRM_CHUNK_SIZE - pp.front.offset;
		} else if (cur_shrm == pp.rear.shrm) {
			size = pp.rear.offset;
			flags = 0;
		} else {
			size = SHRM_CHUNK_SIZE;
		}

		ret = desc_push_back(rts, offset, size, flags, cur_shrm->shrm_id);
		if (first_desc_idx == -1) 
			first_desc_idx = ret;

		if (flags == 0) {
			break;
		}
	}

	pr_info("%s: desc_push_back done", __func__);

	ret = avail_push_back(rts, first_desc_idx);
	if (ret) {
		pr_err("avail_push_back() is failed %d", ret);
		return ret;
	}

	pr_info("%s: avail_push_back done", __func__);

	notify_peer();
	pr_info("%s end", __func__);
	return 0;
}

int get_rw_packet_pos(struct packet_pos* pp, struct rings_to_send* rts, struct shrm_list* rw_shrms, u32 desc_idx) {
	u32 idx;
	if (!pp || !rts || !rts->desc_ring || !rw_shrms) {
		pr_err("%s: input pointers shouldn't be NULL. pp: %#llx, rts: %#llx, rw_shrms: %#llx",
				__func__, pp, rts, rw_shrms);
		return -1;
	}

	if (MAX_DESC_RING <= desc_idx) {
		pr_err("%s: desc_idx is out of idx %d", desc_idx);
		return -2;
	}
	idx = desc_idx;


	pp->front.shrm = get_shrm_with(rw_shrms, rts->desc_ring->ring[idx].shrm_id);
	pp->front.offset = rts->desc_ring->ring[idx].offset;

	for(; rts->desc_ring->ring[idx].flags & IO_RING_DESC_F_NEXT; idx = (idx + 1) % MAX_DESC_RING) {}

	pp->rear.shrm = get_shrm_with(rw_shrms, rts->desc_ring->ring[idx].shrm_id);
	pp->rear.offset = rts->desc_ring->ring[idx].len;

	if (invalid_packet_pos(pp)) {
		pr_err("%s packet_pos is invalid", __func__);
		return -3;
	}

	return 0;
}

int delete_packet(struct rings_to_send* rts, struct shrm_list* rw_shrms) {
	int ret;

	pr_info("%s start", __func__);

	if (!rts || !rts->avail || !rts->peer_used) {
		pr_err("%s: input pointers shouldn't be NULL. rts: %llx", __func__, rts);
		return -1;
	}

	if (!is_empty(rts->peer_used)) {
		int p_used_front = rts->peer_used->front;
		int avail_front = rts->avail->front;
		struct packet_pos pp = {};

		if (p_used_front != avail_front) {
			pr_err("%s: mismatched fronts peer_used->front: %d != avail->front %d",
					__func__, p_used_front, avail_front);
			// TODO: delete_used would be needed. so we need to return a flag for it. it's not an error
			return -2;
		}

		while(rts->peer_used->rear != avail_front) {
			int desc_front = rts->avail->ring[avail_front];
			// Dynmaic shrm allocation & free operations don't use packet.
			// They just use descriptor entry flags (IO_RING_DESC_F_DYN_ALLOC, IO_RING_DESC_F_DYN_FREE)
			bool used_for_dyn_ops = rts->desc_ring->ring[desc_front].flags & IO_RING_DESC_F_DYN_MASK;

			if (rts->peer_used->ring[avail_front] != rts->avail->ring[avail_front]) {
				pr_err("%s: peer_used ring's idx should be matched with avail's one but %d != %d",
						__func__, rts->peer_used->ring[avail_front], rts->avail->ring[avail_front]);
				return -3;
			}

			if (!used_for_dyn_ops) {
				ret = get_rw_packet_pos(&pp, rts, rw_shrms, desc_front);
				if (ret) {
					pr_err("%s: get_rw_packet_pos() failed", __func__);
					return -4;
				}
			}

			ret = avail_pop_front(rts);
			if (ret != avail_front) {
				pr_err("%s: avail_pop_front() failed %d, avail_front %d", __func__, ret, avail_front);
				return ret;
			}

			ret = desc_pop_front(rts);
			if (ret != desc_front) {
				pr_err("%s: desc_pop_front() failed %d, desc_front %d", __func__, ret, desc_front);
				return ret;
			}

			if (!used_for_dyn_ops) {
				ret = delete_packet_from_shrm(&pp, rw_shrms);
				if (ret) {
					pr_err("%s: delete_packet_from_shrm() failed", __func__, ret);
					return ret;
				}
			}

			avail_front = rts->avail->front;
		}
	}

	pr_info("%s done", __func__);
	return 0;
}

int delete_used(struct rings_to_receive* rtr) {
	if (!rtr || !rtr->peer_avail || !rtr->used) {
		pr_err("%s: input pointers shouldn't be NULL. rtr: %llx", __func__, rtr);
		return -1;
	}

	if (is_empty(rtr->used)) {
		pr_info("%s: used ring is empty. so do nothing", __func__);
		return 0;
	}

	if (rtr->peer_avail->front == rtr->used->rear) {
		for(int i = rtr->used->front; i != rtr->used->rear; i = (i + 1) % MAX_DESC_RING) {
			used_pop_front(rtr);
		}
	} else if (rtr->peer_avail->front == rtr->used->front) {
		pr_info("%s: used ring's notification is not handled yet from the peer. so do nothing", __func__);
	} else {
		pr_err("%s: abnormal situation", __func__);

		pr_err("%s: print peer_avail:", __func__);
		print_avail(rtr->peer_avail);
		pr_err("%s: print used:", __func__);
		print_used(rtr->used);

		return -2;
	}

	return 0;
}
