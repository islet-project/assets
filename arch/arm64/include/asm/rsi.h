/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2023 ARM Ltd.
 */

#ifndef __ASM_RSI_H_
#define __ASM_RSI_H_

#include <linux/jump_label.h>
#include <asm/rsi_cmds.h>

#define VIRTQUEUE_NUM 128
struct cloak_iovec {
    unsigned long iov_base;
    unsigned long iov_len;
};

struct p9_pdu_cloak {
    u32         queue_head;
    size_t          read_offset;
    size_t          write_offset;
    u16         out_iov_cnt;
    u16         in_iov_cnt;
    struct cloak_iovec        in_iov[VIRTQUEUE_NUM];
    struct cloak_iovec        out_iov[VIRTQUEUE_NUM];
};

extern struct static_key_false rsi_present;

void arm64_setup_memory(void);

void __init arm64_rsi_init(void);
static inline bool is_realm_world(void)
{
	return static_branch_unlikely(&rsi_present);
}

static inline void set_memory_range(phys_addr_t start, phys_addr_t end,
				    enum ripas state)
{
	unsigned long ret;
	phys_addr_t top;

	while (start != end) {
		ret = rsi_set_addr_range_state(start, end, state, &top);
		BUG_ON(ret);
		BUG_ON(top < start);
		BUG_ON(top > end);
		start = top;
	}
}

static inline void set_memory_range_protected(phys_addr_t start, phys_addr_t end)
{
	set_memory_range(start, end, RSI_RIPAS_RAM);
}

static inline void set_memory_range_shared(phys_addr_t start, phys_addr_t end)
{
	set_memory_range(start, end, RSI_RIPAS_EMPTY);
}
#endif
