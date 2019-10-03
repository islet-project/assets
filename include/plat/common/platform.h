/*
 * Copyright (c) 2018-2019, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __PLATFORM_H__
#define __PLATFORM_H__

#include <stdint.h>
#include <timer.h>
#include <xlat_tables_v2.h>

#define PLAT_PSCI_DUMMY_STATE_ID		0xF

#define PWR_STATE_INIT_INDEX			(-1)

#define INIT_PWR_LEVEL_INDEX(array_name)					\
	do {									\
		unsigned int var;						\
		assert(ARRAY_SIZE(array_name) == (PLAT_MAX_PWR_LEVEL + 1));	\
		for (var = 0; var <= PLAT_MAX_PWR_LEVEL; var++)			\
			array_name[var] = PWR_STATE_INIT_INDEX;			\
	} while (0)

/*
 * The platform structure to represent the valid local power state
 * properties for a particular affinity level. The platform needs to
 * export the array of valid local low power states for each affinity level
 * it supports which can be queried by TFTF tests to construct the required
 * composite power state.
 *
 * TODO: Currently the power levels are identity mapped to affinity level in
 * TFTF which need to be decoupled.
 */
typedef struct plat_state_prop {
	/*
	 * This field has a value in the increasing order of the suspend
	 * depth. Deeper the suspend state, higher the value.
	 */
	unsigned int suspend_depth;
	/* The local state ID for the idle state at this level. */
	unsigned int state_ID;
	/* Flag which indicates whether is a retention or power down state */
	unsigned int is_pwrdown;
} plat_state_prop_t;

void tftf_plat_arch_setup(void);
void tftf_early_platform_setup(void);
void tftf_platform_setup(void);

void tftf_plat_enable_mmu(void);
void tftf_plat_configure_mmu(void);

void tftf_platform_end(void);
void tftf_platform_watchdog_set(void);
void tftf_platform_watchdog_reset(void);

/* Helper that returns a linear core ID from a MPID */
unsigned int platform_get_core_pos(u_register_t mpid);

/* Crash console functions */
int plat_crash_console_init(void);
int plat_crash_console_putc(int c);
int plat_crash_console_flush(void);

/* Gets a handle for the initialised IO entity */
void plat_get_nvm_handle(uintptr_t *handle);

/*
 * Returns the platform topology description array. The size of this
 * array should be PLATFORM_NUM_AFFS - PLATFORM_CORE_COUNT + 1.
 */
const unsigned char *tftf_plat_get_pwr_domain_tree_desc(void);

/*
 * Function to query the MPIDR of a CPU identified by 'core_pos' which is
 * the number returned by platform_get_core() API.
 * In case the CPU is absent, then this API returns INVALID_MPID. This
 * function will be queried only during topology setup in TFTF and thereafter
 * the internal node data will be used to get the MPIDR corresponding
 * to the 'core_pos'.
 */
uint64_t tftf_plat_get_mpidr(unsigned int core_pos);

/*
 * Get the state property array for all the valid states from platform for
 * a specified 'level'. The array is expected to be NULL terminated after the
 * last entry.
 */
const plat_state_prop_t *plat_get_state_prop(unsigned int level);

/*
 * Initialises state info data structures for generating various combinations
 * of state ID's. It also calls tftf_detect_pstate_format() which detects the
 * PSTATE format accepted by EL3 firmware.
 * This function needs to be invoked once during cold boot prior to the
 * invocation of any PSCI power state helper functions.
 */
void tftf_init_pstate_framework(void);

/*
 * This function is used to generate all possible combinations of composite
 * state ID's possible for a given set of power states at each level.
 * Ex: If a system implements 4 levels and each level has 3 local power states.
 * Then, the total combinations of composite power down states possible are:
 *                                                       3 * 3 * 3 * 3 = 81
 *
 * A single call to set_next_state_id_pointers(), sets pointer to pstate_id_idx
 * at all levels for a possible combination out of 81.
 *
 * A caller can confirm when all combinations are completed by checking if
 * pwr_lvel_state_indexes for power_level 0 is PWR_STATE_INIT_INDEX
 */
void tftf_set_next_state_id_idx(unsigned int power_level,
					unsigned int pstate_id_idx[]);

/*
 * This function sets the index for the next state ID of the given power level
 */
void tftf_set_next_local_state_id_idx(unsigned int power_level,
					unsigned int pstate_id_idx[]);

/*
 * This function sets the index corresponding to the deepest power state at
 * a given power level.
 */
void tftf_set_deepest_pstate_idx(unsigned int power_level,
				unsigned int pstate_id_idx[]);

/*
 * Helper function to get the state ID, state type, power level in power_state
 * parameter of CPU_SUSPEND. The generated values are based on the
 * pstate_id_idx values of a core.
 *
 * This helper expects a valid pstate_id_idx till the max valid levels
 * and it detects the max valid level to be terminated by PWR_STATE_INIT value
 *
 * It returns the expected PSCI return value of a suspend request
 */
int tftf_get_pstate_vars(unsigned int *test_power_level,
				unsigned int *test_suspend_type,
				unsigned int *suspend_state_id,
				unsigned int pstate_id_idx[]);

/*
 * This function gets the platform specific timer driver information and
 * initialises platform specific drivers.
 * Returns 0 on success.
 */
int plat_initialise_timer_ops(const plat_timer_t **timer_ops);

struct mem_region {
	uintptr_t addr;
	size_t size;
};

typedef struct mem_region mem_region_t;

/*******************************************************************************
 * Optional functions. A default, weak implementation of those functions is
 * provided, it may be overridden by platform code.
 ******************************************************************************/
unsigned long platform_get_stack(unsigned long mpidr);
/*
 * plat_get_prot_regions: It returns a pointer to a
 * set of regions used to test mem_protect_check.
 * The number of elements are stored in the variable
 *  pointed by nelem.
 */
const mem_region_t *plat_get_prot_regions(int *nelem);

void tftf_plat_reset(void);

const mmap_region_t *tftf_platform_get_mmap(void);

/*
 * Return an IO device handle and specification which can be used
 * to access an image. Use this to enforce platform load policy.
 */
int plat_get_image_source(unsigned int image_id,
		uintptr_t *dev_handle,
		uintptr_t *image_spec);

void plat_fwu_io_setup(void);

#endif /* __PLATFORM_H__ */
