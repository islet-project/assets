/*
 * Copyright (c) 2018-2019, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef SECURE_PARTITION_H
#define SECURE_PARTITION_H

#ifndef __ASSEMBLY__
#include <cassert.h>
#include <param_header.h>
#include <stdint.h>
#include <utils_def.h>
#endif

/*
 * Definitions used to access the members of secure_partition_boot_info from
 * assembly code.
 */
#define SP_BOOT_INFO_STACK_BASE_OFFSET		U(32)
#define SP_BOOT_INFO_IMAGE_SIZE_OFFSET		U(64)
#define SP_BOOT_INFO_PCPU_STACK_SIZE_OFFSET	U(72)

#ifndef __ASSEMBLY__

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
	uint64_t		sp_mem_base;
	uint64_t		sp_mem_limit;
	uint64_t		sp_image_base;
	uint64_t		sp_stack_base;
	uint64_t		sp_heap_base;
	uint64_t		sp_ns_comm_buf_base;
	uint64_t		sp_shared_buf_base;
	uint64_t		sp_image_size;
	uint64_t		sp_pcpu_stack_size;
	uint64_t		sp_heap_size;
	uint64_t		sp_ns_comm_buf_size;
	uint64_t		sp_shared_buf_size;
	uint32_t		num_sp_mem_regions;
	uint32_t		num_cpus;
	secure_partition_mp_info_t *mp_info;
} secure_partition_boot_info_t;

/*
 * This structure is used to pass data associated to secure service requests.
 */
#define SPS_MAX_PAYLOAD_SIZE	32
typedef struct secure_partition_request_info {
	uint32_t		id;
	uint64_t		data_size;
	uint8_t			data[SPS_MAX_PAYLOAD_SIZE];
} secure_partition_request_info_t;

/* Define some fast secure partition requests (SPS) IDs. */
#define SPS_TIMER_SLEEP			1
#define SPS_CHECK_ALIVE			2

#define CACTUS_FAST_REQUEST_SUCCESS	0xCACF900D

secure_partition_request_info_t *create_sps_request(uint32_t id,
						    const void *data,
						    uint64_t data_size);

/*
 * Compile time assertions related to the 'secure_partition_boot_info' structure
 * to ensure that the assembler and the compiler view of the offsets of the
 * structure members is the same.
 */
CASSERT(SP_BOOT_INFO_STACK_BASE_OFFSET ==
	__builtin_offsetof(secure_partition_boot_info_t, sp_stack_base), \
	assert_secure_partition_boot_info_sp_stack_base_offset_mismatch);

CASSERT(SP_BOOT_INFO_IMAGE_SIZE_OFFSET ==
	__builtin_offsetof(secure_partition_boot_info_t, sp_image_size), \
	assert_secure_partition_boot_info_sp_image_size_offset_mismatch);

CASSERT(SP_BOOT_INFO_PCPU_STACK_SIZE_OFFSET ==
	__builtin_offsetof(secure_partition_boot_info_t, sp_pcpu_stack_size), \
	assert_secure_partition_boot_info_sp_pcpu_stack_size_offset_mismatch);

#endif /* __ASSEMBLY__ */

#endif /* SECURE_PARTITION_H */
