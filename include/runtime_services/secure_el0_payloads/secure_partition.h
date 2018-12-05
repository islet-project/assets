/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef SECURE_PARTITION_H
#define SECURE_PARTITION_H

#include <param_header.h>
#include <types.h>
#include <utils_def.h>

/*
 * Flags used by the secure_partition_mp_info structure to describe the
 * characteristics of a cpu. Only a single flag is defined at the moment to
 * indicate the primary cpu.
 */
#define MP_INFO_FLAG_PRIMARY_CPU	U(0x00000001)

/*
 * This structure is used to provide information required to initialise a S-EL0
 * partition.
 */
typedef struct secure_partition_mp_info {
	uint64_t		mpidr;
	uint32_t		linear_id;
	uint32_t		flags;
} secure_partition_mp_info_t;

typedef struct secure_partition_boot_info {
	param_header_t		h;
	uint32_t		num_cpus;
	secure_partition_mp_info_t *mp_info;
} secure_partition_boot_info_t;


#endif /* SECURE_PARTITION_H */
