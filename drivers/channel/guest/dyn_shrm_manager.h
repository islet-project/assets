#ifndef _DYN_SHRM_MANAGER_H
#define _DYN_SHRM_MANAGER_H

#include <linux/list.h>
#include "io_ring.h"
#include "shrm.h"
#ifndef CONFIG_GUEST_CHANNEL_IO_RING
#include <linux/genalloc.h>
#endif

#define MIN_FREE_SHRM_SIZE 1024 * 8 // 8kb
//#define MIN_FREE_SHRM_SIZE 1024 * 1024 * 2 // 2MB
#define MAX_FREE_SHRM_SIZE 1024 * 16 // 16kb
//#define MAX_FREE_SHRM_SIZE 1024 * 1024 * 4 // 4MB

struct shrm_list {
	struct list_head head;
	struct packet_pos pp;
	u64 free_size, total_size;
	u64 ipa_start, ipa_end; // RW shrm IPA range only for the current realm
	bool add_req_pending;
#ifndef CONFIG_CHANNL_GUEST_IO_RING
	struct gen_pool* shrm_pool;
#endif
	//TODO: need lock to enlarge and shrink it
};

struct shrm_list* init_shrm_list(struct rings_to_send* rts, u64 ipa_start, u64 ipa_size);
int remove_shrm_chunk(struct shrm_list* rw_shrms, u64 ipa);
int write_to_shrm(struct rings_to_send* rts, struct shrm_list* rw_shrms, struct packet_pos* pp, const void* data, u64 size);
int copy_from_shrm(void* to, struct packet_pos* from);
int add_rw_shrm_chunk(struct rings_to_send* rts, struct shrm_list* rw_shrms, s64 shrm_ipa, u32 shrm_id);
int add_ro_shrm_chunk(struct list_head* ro_shrms_head, u32 shrm_id);
s64 req_shrm_chunk(struct rings_to_send* rts, struct shrm_list* rw_shrms);
bool invalid_packet_pos(struct packet_pos* pp);
u64 req_ro_shrm_ipa(u32 shrm_id);
void print_front_rear(struct packet_pos* pp);
struct shared_realm_memory* get_shrm_with(struct shrm_list* rw_shrms, u32 shrm_id);
int delete_packet_from_shrm(struct packet_pos* pp, struct shrm_list* rw_shrms);

#endif /* _DYN_SHRM_MANAGER_H */
