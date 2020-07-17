/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef SP_HELPERS_H
#define SP_HELPERS_H

#include <stdint.h>
#include <tftf_lib.h>
#include <ffa_helpers.h>

#define SPM_VM_ID_FIRST                 (1)
#define SPM_VM_ID_SECOND                (2)
#define SPM_VM_ID_THIRD                 (3)

#define SPM_VM_GET_COUNT                (0xFF01)
#define SPM_VCPU_GET_COUNT              (0xFF02)
#define SPM_DEBUG_LOG                   (0xBD000000)

typedef struct {
	u_register_t fid;
	u_register_t arg1;
	u_register_t arg2;
	u_register_t arg3;
	u_register_t arg4;
	u_register_t arg5;
	u_register_t arg6;
	u_register_t arg7;
} svc_args;

/*
 * Trigger an SVC call.
 *
 * The arguments to pass through the SVC call must be stored in the svc_args
 * structure. The return values of the SVC call will be stored in the same
 * structure (overriding the input arguments).
 *
 * Return the first return value. It is equivalent to args.fid but is also
 * provided as the return value for convenience.
 */
u_register_t sp_svc(svc_args *args);

/*
 * Choose a pseudo-random number within the [min,max] range (both limits are
 * inclusive).
 */
uintptr_t bound_rand(uintptr_t min, uintptr_t max);

/*
 * Check that expr == expected.
 * If not, loop forever.
 */
void expect(int expr, int expected);

/*
 * Test framework functions
 */

void announce_test_section_start(const char *test_sect_desc);
void announce_test_section_end(const char *test_sect_desc);

void announce_test_start(const char *test_desc);
void announce_test_end(const char *test_desc);

/* Sleep for at least 'ms' milliseconds. */
void sp_sleep(uint32_t ms);

/*
 * Hypervisor Calls Wrappers
 */

ffa_vcpu_count_t spm_vcpu_get_count(ffa_vm_id_t vm_id);

ffa_vm_count_t spm_vm_get_count(void);

void spm_debug_log(char c);

#endif /* SP_HELPERS_H */
