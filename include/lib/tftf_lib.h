/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __TFTF_LIB_H__
#define __TFTF_LIB_H__

#ifndef __ASSEMBLY__

#include <arch.h>
#include <arch_helpers.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>

/*
 * Possible error codes for signaling the result of a test
 * TEST_RESULT_MIN and TEST_RESULT_MAX are only used as bounds in the enum.
 */
typedef enum {
	/*
	 * NA = Not applicable.
	 * Initial value for a test result.
	 * Used for CPUs that don't participate in the test.
	 */
	TEST_RESULT_NA		= -1,

	TEST_RESULT_MIN		= 0,
	TEST_RESULT_SKIPPED	= TEST_RESULT_MIN,
	TEST_RESULT_SUCCESS,
	TEST_RESULT_FAIL,
	TEST_RESULT_CRASHED,

	TEST_RESULT_MAX
} test_result_t;

#define TEST_RESULT_IS_VALID(result) \
	((result >= TEST_RESULT_MIN) && (result < TEST_RESULT_MAX))

/*
 * PSCI Function Wrappers
 *
 * SMC calls to PSCI functions
 */
int32_t tftf_psci_cpu_on(u_register_t target_cpu,
			 uintptr_t entry_point_address,
			 u_register_t context_id);
int32_t tftf_psci_cpu_off(void);
int32_t tftf_psci_affinity_info(u_register_t target_affinity,
				uint32_t lowest_affinity_level);
int32_t tftf_psci_node_hw_state(u_register_t target_cpu, uint32_t power_level);
int32_t tftf_get_psci_feature_info(uint32_t psci_func_id);
u_register_t tftf_psci_stat_count(u_register_t target_cpu,
		uint32_t power_state);
u_register_t tftf_psci_stat_residency(u_register_t target_cpu,
		uint32_t power_state);

/*
 * PSCI Helper functions
 */

/*
 * Gets the context ID used when calling tftf_psci_cpu_on().
 */
u_register_t tftf_get_cpu_on_ctx_id(unsigned int core_pos);

/*
 * Sets the context ID used when calling tftf_psci_cpu_on().
 */
void tftf_set_cpu_on_ctx_id(unsigned int core_pos, u_register_t context_id);

/*
 * Gets the PSCI version of Trusted Firmware-A. The version number returned
 * is a 32-bit unsigned integer, with the upper 16 bits denoting the major
 * revision, and the lower 16 bits denoting the minor revision.
 */
unsigned int tftf_get_psci_version(void);

/*
 * Returns 0 if version is not a valid PSCI version supported by TFTF.
 * Otherwise it returns a value different of 0.
 */
int tftf_is_valid_psci_version(unsigned int version);


/*
 * The function constructs a composite state_id up-to the specified
 * affinity level querying the relevant state property from the platform.
 * It chooses the first matching state property from the array returned
 * by platform. In case the requested affinity level is not supported by
 * the platform, then this function uses DUMMY_STATE_ID as the local state
 * for that level. This allows the tests to construct composite state-id
 * for invalid affinity levels as well. It returns the expected return
 * value from CPU SUSPEND call.
 */
int tftf_psci_make_composite_state_id(uint32_t affinity_level,
		uint32_t state_type, uint32_t *state_id);

/*
 * This function composes the power state parameter in the right format
 * needed by PSCI. The detection of the power state format is done during
 * cold boot by tftf_detect_psci_pstate_format() function.
 */
uint32_t tftf_make_psci_pstate(uint32_t affinity_level,
					uint32_t state_type,
					uint32_t state_id);

/*
 * Returns 1, if the EL3 software supports PSCI's original format state ID as
 * NULL else returns zero
 */
unsigned int tftf_is_psci_state_id_null(void);

/*
 * Returns 1, if the EL3 software supports PSCI's original state format else
 * returns zero
 */
unsigned int tftf_is_psci_pstate_format_original(void);

/* Functions to wait for a specified number of ms or us */
void waitms(uint64_t ms);
void waitus(uint64_t us);

/*
 * SMC calls take a function identifier and up to 6 arguments.
 * Additionally, for SMC calls that originate from EL2, an optional seventh
 * argument can be added. Given that TFTF runs in EL2, we need to be able to
 * specify it.
 */
typedef struct {
	/* Function identifier. Identifies which function is being invoked. */
	uint32_t	arg0;

	u_register_t	arg1;
	u_register_t	arg2;
	u_register_t	arg3;
	u_register_t	arg4;
	u_register_t	arg5;
	u_register_t	arg6;
	u_register_t	arg7;
} smc_args;

/* SMC calls can return up to 4 register values */
typedef struct {
	u_register_t	ret0;
	u_register_t	ret1;
	u_register_t	ret2;
	u_register_t	ret3;
} smc_ret_values;

/*
 * Trigger an SMC call.
 */
smc_ret_values tftf_smc(const smc_args *args);

/*
 * Write a formatted string in the test output buffer.
 * Just like the standard libc's printf() function, the string produced is under
 * the control of a format string that specifies how subsequent arguments are
 * converted.
 *
 * The string will appear in the test report.
 * Use mp_printf() instead for volatile debug messages that are not meant to be
 * stored into the test report.
 * Note: The test output buffer referred here is a temporary buffer stored in
 * RAM. This function doesn't write anything into NVM.
 *
 * Upon successful return, return the number of characters printed (not
 * including the final '\0' character). If an output error is encountered,
 * a negative value is returned. If the function is not able to print any
 * character at all, this is considered as an output error. Note that a partial
 * write (i.e. when the string is truncated) is not considered as an output
 * error.
 */
__attribute__((format(printf, 1, 2)))
int tftf_testcase_printf(const char *format, ...);

/*
 * This function is meant to be used by tests.
 * It tells the framework that the test is going to reset the platform.
 *
 * It the test omits to call this function before resetting, the framework will
 * consider the test has crashed upon resumption.
 */
void tftf_notify_reboot(void);

/*
 * Returns 0 if the test function is executed for the first time,
 *      or 1 if the test rebooted the platform and the test function is being
 *         executed again.
 * This function is used for tests that reboot the platform, so that they can
 * execute different code paths on 1st execution and subsequent executions.
 */
unsigned int tftf_is_rebooted(void);

static inline unsigned int make_mpid(unsigned int clusterid,
				     unsigned int coreid)
{
	/*
	 * If MT bit is set then need to shift the affinities and also set the
	 * MT bit.
	 */
	if ((read_mpidr_el1() & MPIDR_MT_MASK) != 0)
		return MPIDR_MT_MASK |
			((clusterid & MPIDR_AFFLVL_MASK) << MPIDR_AFF2_SHIFT) |
			((coreid & MPIDR_AFFLVL_MASK) << MPIDR_AFF1_SHIFT);
	else
		return ((clusterid & MPIDR_AFFLVL_MASK) << MPIDR_AFF1_SHIFT) |
			((coreid & MPIDR_AFFLVL_MASK) << MPIDR_AFF0_SHIFT);

}

#endif /* __ASSEMBLY__ */
#endif /* __TFTF_LIB_H__ */
