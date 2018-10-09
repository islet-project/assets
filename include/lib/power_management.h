/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __POWER_MANAGEMENT_H__
#define __POWER_MANAGEMENT_H__

#include <platform_def.h>
#include <psci.h>
#include <spinlock.h>
#include <stdint.h>
#include <types.h>

/* Set of states of an affinity node as seen by the Test Framework */
typedef enum {
	TFTF_AFFINITY_STATE_OFF = 0,
	TFTF_AFFINITY_STATE_ON_PENDING,
	TFTF_AFFINITY_STATE_ON,
} tftf_affinity_info_t;

/* Structure for keeping track of CPU state */
typedef struct {
	volatile tftf_affinity_info_t state;
	spinlock_t lock;
} __aligned(CACHE_WRITEBACK_GRANULE) tftf_cpu_state_t;

/*
 * Suspend information passed to the TFTF suspend helpers.
 */
typedef struct suspend_info {
	/* The power state parameter to be passed to PSCI_CPU_SUSPEND */
	unsigned int power_state;
	/* SMC function ID of the PSCI suspend call */
	unsigned int psci_api;
	/* Whether the system context needs to be saved and restored */
	unsigned int save_system_context;
} suspend_info_t;

/*
 * Power up a core.
 * This uses the PSCI CPU_ON API, which means it relies on the EL3 firmware's
 * runtime services capabilities.
 * The core will be boostrapped by the framework before handing it over
 * to the entry point specified as the 2nd argument.
 *
 *    target_cpu: MPID of the CPU to power up
 *    entrypoint: Address where the CPU will jump once the framework has
 *                initialized it
 *    context_id: Context identifier as defined by the PSCI specification
 *
 *    Return: Return code of the PSCI CPU_ON call
 *            (refer to the PSCI specification for details)
 */
int32_t tftf_cpu_on(u_register_t target_cpu,
		    uintptr_t entrypoint,
		    u_register_t context_id);

/*
 * Tries to power up a core.
 * This API is similar to tftf_cpu_on API with the difference being it
 * does a SMC call to EL3 firmware without checking the status of the
 * core with respect to the framework.
 *
 * A caller is expected to handle the return code given by the EL3 firmware.
 *
 *    target_cpu: MPID of the CPU to power up
 *    entrypoint: Address where the CPU will jump once the framework has
 *                initialised it
 *    context_id: Context identifier as defined by the PSCI specification
 *
 *    Return: Return code of the PSCI CPU_ON call
 *            (refer to the PSCI specification for details)
 */
int32_t tftf_try_cpu_on(u_register_t target_cpu,
		    uintptr_t entrypoint,
		    u_register_t context_id);

/*
 * Power down the calling core.
 * This uses the PSCI CPU_OFF API, which means it relies on the EL3 firmware's
 * runtime services capabilities.
 *
 *    Return: This function does not return when successful.
 *            Otherwise, return the same error code as the PSCI CPU_OFF call
 *            (refer to the PSCI specification for details)
 */
int32_t tftf_cpu_off(void);

/*
 * It is an Api used to enter a suspend state. It does the following:
 * - Allocates space for saving architectural and non-architectural CPU state on
 *   stack
 * - Saves architecture state of the CPU in the space allocated which consists:
 *      a. Callee registers
 *      b. System control registers. ex: MMU, SCTLR_EL1
 * - Depending on the state of `save_system_context` flag in suspend_info
 *   saves the context of system peripherals like GIC, timer etc.
 * - Sets context ID to the base of the stack allocated for saving context
 * - Calls Secure Platform Firmware to enter suspend
 * - If suspend fails, It restores the callee registers
 *     power state: PSCI power state to be sent via SMC
 *     Returns: PSCI_E_SUCCESS or PSCI_E_INVALID_PARAMS
 *
 * Note: This api might not test all use cases, as the context ID and resume
 * entrypoint is in the control of the framework.
 */
int tftf_suspend(const suspend_info_t *info);


/* ----------------------------------------------------------------------------
 * The above APIs might not be suitable in all test scenarios.
 * A test case could want to bypass those APIs i.e. call the PSCI APIs
 * directly. In this case, it is the responsibility of the test case to preserve
 * the state of the framework. The below APIs are provided to this end.
 * ----------------------------------------------------------------------------
 */

/*
 * The 3 following functions are used to manipulate the reference count tracking
 * the number of CPUs participating in a test.
 */

/*
 * Increment the reference count.
 * Return the new, incremented value.
 */
unsigned int tftf_inc_ref_cnt(void);

/*
 * Decrement the reference count.
 * Return the new, decremented value.
 */
unsigned int tftf_dec_ref_cnt(void);

/* Return the current reference count value */
unsigned int tftf_get_ref_cnt(void);

/*
 * Set the calling CPU online/offline. This only adjusts the view of the core
 * from the framework's point of view, it doesn't actually power up/down the
 * core.
 */
void tftf_set_cpu_online(void);
void tftf_init_cpus_status_map(void);
void tftf_set_cpu_offline(void);

/*
 * Query the state of a core.
 *   Return: 1 if the core is online, 0 otherwise.
 */
unsigned int tftf_is_cpu_online(unsigned int mpid);

unsigned int tftf_is_core_pos_online(unsigned int core_pos);

/* TFTF Suspend helpers */
static inline int tftf_cpu_suspend(unsigned int pwr_state)
{
	suspend_info_t info = {
		.power_state = pwr_state,
		.save_system_context = 0,
		.psci_api = SMC_PSCI_CPU_SUSPEND,
	};

	return tftf_suspend(&info);
}

static inline int tftf_cpu_suspend_save_sys_ctx(unsigned int pwr_state)
{
	suspend_info_t info = {
		.power_state = pwr_state,
		.save_system_context = 1,
		.psci_api = SMC_PSCI_CPU_SUSPEND,
	};

	return tftf_suspend(&info);
}


static inline int tftf_system_suspend(void)
{
	suspend_info_t info = {
		.power_state = 0,
		.save_system_context = 1,
		.psci_api = SMC_PSCI_SYSTEM_SUSPEND,
	};

	return tftf_suspend(&info);
}

#endif /* __POWER_MANAGEMENT_H__ */
