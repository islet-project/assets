#ifndef _DYN_SHRM_MANAGER_H
#define _DYN_SHRM_MANAGER_H

#include <linux/list.h>

#define SHRM_CHUNK_SIZE PAGE_SIZE // TODO: need to defined by config 
#define MIN_FREE_SHRM_SIZE 1024 * 1024 * 2 // 2MB
#define MAX_FREE_SHRM_SIZE 1024 * 1024 * 4 // 4MB

struct shared_realm_memory {
	struct list_head head;
	u64 ipa;
};

struct pos {
	struct shared_realm_memory *shrm;
	u64 offset;
};

struct packet_pos {
	struct pos front, rear;
	u64 size;
};

struct shrm_list {
	struct list_head head;
	struct packet_pos pp;
	u64 free_size, total_size;
	u64 ipa_start, ipa_end; // RW shrm IPA range only for the current realm
	bool add_req_pending;
};

int init_shrm_list(u64, u64);
int write_to_shrm(struct packet_pos* pp, const void* data, u64 size);
int copy_from_shrm(void* to, struct packet_pos* from);
int add_shrm_chunk(void);

#endif /* _DYN_SHRM_MANAGER_H */

