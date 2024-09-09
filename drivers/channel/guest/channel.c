#include "dyn_shrm_manager.h"
#include "shrm.h"
#include "channel.h"
#include <linux/printk.h>

void notify_peer(void);

// TODO: implement it
int read_packet(struct rings_to_receive* rtr, struct list_head* ro_shrms_head) {
	int ret;
	struct io_ring* peer_avail;
	struct desc_ring* peer_desc_ring;
	u16 i;

	pr_info("%s start", __func__);

	pr_info("%s: peer_avail %llx, peer_desc_ring %llx",
			__func__, rtr->peer_avail, rtr->peer_desc_ring);

	if (!rtr || !ro_shrms_head) {
		pr_err("%s: input pointers shouldn't be NULL. rtr: %llx, ro_shrms_head: %llx",
				__func__, rtr, ro_shrms_head);
		return -1;
	}

	peer_avail = rtr->peer_avail;
	peer_desc_ring = rtr->peer_desc_ring;
	if (!peer_avail || !peer_desc_ring) {
		pr_err("%s: input pointers shouldn't be NULL. peer_avail: %llx, peer_desc_ring: %llx",
				__func__, peer_avail, peer_desc_ring);
	}

	i = peer_avail->front;
	pr_info("%s: peer_avail->front %d", __func__, i);
	if (MAX_DESC_RING <= i) {
		pr_err("%s: peer_avail->front %d shouldn't bigger than %d",
				__func__, i, MAX_DESC_RING);
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
				}
			} else if (desc->flags & IO_RING_DESC_F_DYN_FREE) {
				// TODO: remove_ro_shrm_chunk();
			} else {
				ret = read_desc(desc, ro_shrms_head);
				if (ret) {
					pr_err("%s: read_desc failed %d", __func__, ret);
					return ret;
				}
			}
			
		} while(desc->flags & IO_RING_DESC_F_NEXT);

		i = (i + 1) % MAX_DESC_RING;
	}

	//notify_peer();
	pr_info("%s done", __func__);
	return 0;
}

int write_packet(struct rings_to_send* rts, struct shrm_list* rw_shrms, const void* data, u64 size) {
	int ret, first_desc_idx = -1;
	struct packet_pos pp = {};
	struct shared_realm_memory* cur_shrm = NULL;

	pr_info("%s start", __func__);

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
