#include "dyn_shrm_manager.h"
#include "shrm.h"
#include "channel.h"
#include <linux/printk.h>

void notify_peer(void);

int write_packet(struct rings_to_send* rts, struct shrm_list* rw_shrms, const void* data, u64 size) {
	int ret, first_desc_idx = -1;
	struct packet_pos pp = {};
	struct shared_realm_memory* cur_shrm = NULL;
	u16 flags = IO_RING_DESC_F_NEXT;

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
		if (pp.front.shrm == pp.rear.shrm) {
			size = pp.rear.offset - pp.front.offset;
			flags = 0;
		} else if (cur_shrm == pp.rear.shrm) {
			size = pp.rear.offset;
			flags = 0;
		} else {
			size = SHRM_CHUNK_SIZE - pp.front.offset;
		}
		ret = desc_push_back(rts, cur_shrm->ipa, size, flags);
		if (first_desc_idx == -1) 
			first_desc_idx = ret;

		if (cur_shrm == pp.rear.shrm) {
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
