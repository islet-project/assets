#ifndef _CHANNEL_H
#define _CHANNEL_H

#include "io_ring.h"

int write_packet(struct rings_to_send* rts, struct shrm_list* rw_shrms, const void* data, u64 size);
int read_packet(struct rings_to_receive* rtr, struct list_head* ro_shrms_head);

#endif /* _CHANNEL_H */
