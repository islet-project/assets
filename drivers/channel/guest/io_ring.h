#ifndef _IO_RING_H
#define _IO_RING_H
#include <linux/types.h>
#include <linux/limits.h>

#define MAX_DESC_RING 128

/*
 * chained descriptor should set this flag.
 *
 * the driver can also chain more than one descriptor.
 * if the next (0x1) flag of a descriptor is set, the data continue in the next entry,
 * making a chain of descriptors. 
 */
#define IO_RING_DESC_F_NEXT 1 

struct rings_to_send {
	struct io_ring *avail; // RW ring
	struct io_ring *peer_used; // RO ring. get noti by the first irq
	struct desc_ring *desc_ring; // RW ring
};

struct rings_to_receive {
	struct io_ring *peer_avail; // RO ring
	struct io_ring *used; // RW ring
	struct desc_ring *peer_desc_ring; // RO ring
};

/*
 * circular queue for inter realm i/o
 * front     : indicate a index of the ring to remove
 * rear      : indicate a index of the ring to add a new descriptor ring's index
 * noti_limit: if an rear is matched with the noti_limit, then notify the peer
 * ring[]    : indicate a index of the descriptor ring
 */
struct io_ring {
	u16 front, rear;
	u16 noti_limit;
	u64 shrm_ipa_base; // ipa of the shared realm memory
	u16 ring[MAX_DESC_RING];
};

struct desc {
	u64 ipa;
	u32 len;
	u16 flags;
};

struct desc_ring {
	u16 front, rear;
	struct desc ring[MAX_DESC_RING];
};


int avail_push_back(struct rings_to_send* rings_to_send, u16 desc_idx);
int used_push_back(struct rings_to_receive* rings_to_recv, u16 desc_idx);
int desc_push_back(struct rings_to_send* rings_to_send, u64 ipa, u32 len, u16 flags);
void init_rings_to_send(struct rings_to_send* rts, u64 shrm_ipa, u64* shrm_va);

#endif /* _IO_RING_H */
