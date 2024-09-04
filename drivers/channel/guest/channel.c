#include "dyn_shrm_manager.h"
#include "shrm.h"
#include "channel.h"
#include <linux/printk.h>

void notify_peer(void);

// TODO: implement it
int read_packet(struct rings_to_receive* rtr) {
	struct io_ring* peer_used = rtr->peer_used;
	struct io_ring* peer_desc_ring = rtr->peer_desc_ring;
	u16 i = peer_used->front;
	int ret;

	while(i != peer_used->rear) {
		u16 desc_idx = peer_used->ring[i];
		struct desc* desc = peer_desc_ring->ring[desc_idx];

		ret = read_desc(desc);
		if (!ret) {
			pr_err("%s: read_desc failed %d", __func__, ret);
			return ret;
		}
		i = (i + 1) % MAX_DESC_RING;
	}
	return 0;
}

int write_packet(struct rings_to_send* rts, struct shrm_list* rw_shrms, const void* data, u64 size) {
	int ret, first_desc_idx = -1;
	struct packet_pos pp = {};
	struct shared_realm_memory* cur_shrm = NULL;

	pr_info("%s start", __func__);

	do {
		ret = write_to_shrm(rw_shrms, &pp, data, size);
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

	list_for_each_entry(cur_shrm, &pp.front.shrm->head, head) {
		u64 offset = 0;
		u16 flags = IO_RING_DESC_F_NEXT;

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

		ret = desc_push_back(rts, offset, size, cur_shrm->shrm_id, flags);
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
