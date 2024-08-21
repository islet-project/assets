#include "io_ring.h"
#include "dyn_shrm_manager.h"
#include "shared_realm_memory.h"

void notify_peer(void);

int write_packet(struct rings_to_send* rings_to_send, struct shrm_list* rw_shrms, const void* data, u64 size) {
	int ret, first_desc_idx = -1;
	struct packet_pos pp = {};
	struct shared_realm_memory* cur_shrm = NULL;
	u16 flags = IO_RING_DESC_F_NEXT;

	do {
		ret = write_to_shrm(&pp, data, size);
	} while(ret == -EAGAIN);

	if (ret < 0) {
		pr_err("%s: write_to_shrm() is failed", __func__);
		return ret;
	}

	if (!invalid_packet_pos(pp)) {
		pr_err("%s: invalid packet_pos", __func__);
		return -EINVAL;
	}

	list_for_each_entry(cur_shrm, &pp->front.shrm.head, head) {
		if (pp->front.shrm == pp->rear.shrm) {
			size = pp->rear.shrm.offset - pp->front.shrm.offset;
			flags = 0;
		} else if (cur_shrm == pp->rear.shrm) {
			size = cur_shrm->offset;
			flags = 0;
		} else {
			size = SHRM_CHUNK_SIZE - cur_shrm->offset;
		}
		ret = desc_push_back(rings_to_send, cur_shrm->ipa, size, flags);
		if (first_desc_idx == -1) 
			first_desc_idx = ret;

		if (cur_shrm == pp->rear.shrm) {
			break;
		}
	}

	ret = avail_push_back(rings_to_send, first_desc_idx);
	if (ret) {
		pr_err("avail_push_back() is failed %d", ret);
		return ret;
	}

	notify_peer();

	return 0;
}











