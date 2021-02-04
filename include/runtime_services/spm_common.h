/*
 * Copyright (c) 2021, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef SPM_COMMON_H
#define SPM_COMMON_H

#include <ffa_helpers.h>
#include <stdint.h>
#include <string.h>

/* Hypervisor ID at physical FFA instance */
#define HYP_ID          (0)

/*
 * The bit 15 of the FF-A ID indicates whether the partition is executing
 * in the normal world, in case it is a Virtual Machine (VM); or in the
 * secure world, in case it is a Secure Partition (SP).
 *
 * If bit 15 is set partition is an SP; if bit 15 is clear partition is
 * a VM.
 */
#define SP_ID_MASK	U(1 << 15)
#define SP_ID(x)	((x) | SP_ID_MASK)
#define IS_SP_ID(x)	((x & SP_ID_MASK) != 0U)

struct ffa_features_test {
	const char *test_name;
	unsigned int feature;
	unsigned int expected_ret;
};
unsigned int get_ffa_feature_test_target(const struct ffa_features_test **test_target);

struct mailbox_buffers {
	void *recv;
	void *send;
};

#define CONFIGURE_MAILBOX(mb_name, buffers_size) 				\
	do {									\
	/* Declare RX/TX buffers at virtual FF-A instance */			\
	static struct {								\
			uint8_t rx[buffers_size];				\
			uint8_t tx[buffers_size];				\
	} __aligned(PAGE_SIZE) mb_buffers;					\
	mb_name.recv = (void *)mb_buffers.rx;					\
	mb_name.send = (void *)mb_buffers.tx;					\
	} while (false)

#define CONFIGURE_AND_MAP_MAILBOX(mb_name, buffers_size, smc_ret)		\
	do {									\
	CONFIGURE_MAILBOX(mb_name, buffers_size);				\
	smc_ret = ffa_rxtx_map(							\
				(uintptr_t)mb_name.send,			\
				(uintptr_t)mb_name.recv, 			\
				buffers_size / PAGE_SIZE			\
			);							\
	} while (false)

bool check_spmc_execution_level(void);

#endif /* SPM_COMMON_H */
