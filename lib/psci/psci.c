/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <arch_helpers.h>
#include <debug.h>
#include <drivers/arm/arm_gic.h>
#include <irq.h>
#include <platform.h>
#include <power_management.h>
#include <psci.h>
#include <sgi.h>
#include <tftf.h>
#include <tftf_lib.h>

static unsigned int pstate_format_detected;
static unsigned int pstate_format;
static unsigned int is_state_id_null;

const psci_function_t psci_functions[PSCI_NUM_CALLS] = {
	DEFINE_PSCI_FUNC(PSCI_FEATURES, true),
	DEFINE_PSCI_FUNC(PSCI_VERSION, true),
	DEFINE_PSCI_FUNC(PSCI_CPU_SUSPEND_AARCH32, true),
	DEFINE_PSCI_FUNC(PSCI_CPU_SUSPEND_AARCH64, true),
	DEFINE_PSCI_FUNC(PSCI_CPU_OFF, true),
	DEFINE_PSCI_FUNC(PSCI_CPU_ON_AARCH32, true),
	DEFINE_PSCI_FUNC(PSCI_CPU_ON_AARCH64, true),
	DEFINE_PSCI_FUNC(PSCI_AFFINITY_INFO_AARCH32, true),
	DEFINE_PSCI_FUNC(PSCI_AFFINITY_INFO_AARCH64, true),
	DEFINE_PSCI_FUNC(PSCI_SYSTEM_OFF, true),
	DEFINE_PSCI_FUNC(PSCI_SYSTEM_RESET, true),
	DEFINE_PSCI_FUNC(PSCI_MIG_INFO_TYPE, false),
	DEFINE_PSCI_FUNC(PSCI_MIG_INFO_UP_CPU_AARCH32, false),
	DEFINE_PSCI_FUNC(PSCI_MIG_INFO_UP_CPU_AARCH64, false),
	DEFINE_PSCI_FUNC(PSCI_MIG_AARCH32, false),
	DEFINE_PSCI_FUNC(PSCI_MIG_AARCH64, false),
	DEFINE_PSCI_FUNC(PSCI_CPU_FREEZE, false),
	DEFINE_PSCI_FUNC(PSCI_CPU_DEFAULT_SUSPEND32, false),
	DEFINE_PSCI_FUNC(PSCI_CPU_DEFAULT_SUSPEND64, false),
	DEFINE_PSCI_FUNC(PSCI_CPU_HW_STATE32, false),
	DEFINE_PSCI_FUNC(PSCI_CPU_HW_STATE64, false),
	DEFINE_PSCI_FUNC(PSCI_SYSTEM_SUSPEND32, false),
	DEFINE_PSCI_FUNC(PSCI_SYSTEM_SUSPEND64, false),
	DEFINE_PSCI_FUNC(PSCI_SET_SUSPEND_MODE, false),
	DEFINE_PSCI_FUNC(PSCI_STAT_RESIDENCY32, false),
	DEFINE_PSCI_FUNC(PSCI_STAT_RESIDENCY64, false),
	DEFINE_PSCI_FUNC(PSCI_STAT_COUNT32, false),
	DEFINE_PSCI_FUNC(PSCI_STAT_COUNT64, false),
	DEFINE_PSCI_FUNC(PSCI_MEM_PROTECT, false),
	DEFINE_PSCI_FUNC(PSCI_MEM_PROTECT_CHECK_RANGE32, false),
	DEFINE_PSCI_FUNC(PSCI_MEM_PROTECT_CHECK_RANGE64, false),
	DEFINE_PSCI_FUNC(PSCI_RESET2_AARCH32, false),
	DEFINE_PSCI_FUNC(PSCI_RESET2_AARCH64, false),
};

int32_t tftf_psci_cpu_on(u_register_t target_cpu,
			 uintptr_t entry_point_address,
			 u_register_t context_id)
{
	smc_args args = {
		SMC_PSCI_CPU_ON,
		target_cpu,
		entry_point_address,
		context_id
	};
	smc_ret_values ret_vals;

	ret_vals = tftf_smc(&args);

	return ret_vals.ret0;
}

int32_t tftf_psci_cpu_off(void)
{
	smc_args args = { SMC_PSCI_CPU_OFF };
	smc_ret_values ret_vals;

	ret_vals = tftf_smc(&args);
	return ret_vals.ret0;
}


u_register_t tftf_psci_stat_residency(u_register_t target_cpu,
		uint32_t power_state)
{
	smc_args args = {
		SMC_PSCI_STAT_RESIDENCY,
		target_cpu,
		power_state,
	};
	smc_ret_values ret_vals;

	ret_vals = tftf_smc(&args);
	return ret_vals.ret0;
}

u_register_t tftf_psci_stat_count(u_register_t target_cpu,
		uint32_t power_state)
{
	smc_args args = {
		SMC_PSCI_STAT_COUNT,
		target_cpu,
		power_state,
	};
	smc_ret_values ret_vals;

	ret_vals = tftf_smc(&args);
	return ret_vals.ret0;
}

int32_t tftf_psci_affinity_info(u_register_t target_affinity,
				uint32_t lowest_affinity_level)
{
	smc_ret_values ret_vals;

	smc_args args = {
			   SMC_PSCI_AFFINITY_INFO,
			   target_affinity,
			   lowest_affinity_level
			  };

	ret_vals = tftf_smc(&args);
	return ret_vals.ret0;
}

int32_t tftf_psci_node_hw_state(u_register_t target_cpu, uint32_t power_level)
{
	smc_args args = {
		SMC_PSCI_CPU_HW_STATE,
		target_cpu,
		power_level
	};
	smc_ret_values ret;

	ret = tftf_smc(&args);
	return ret.ret0;
}

int32_t tftf_get_psci_feature_info(uint32_t psci_func_id)
{
	smc_args args = {
		SMC_PSCI_FEATURES,
		psci_func_id
	};
	smc_ret_values ret;

	ret = tftf_smc(&args);
	return ret.ret0;
}

int tftf_psci_make_composite_state_id(uint32_t affinity_level,
		uint32_t state_type, uint32_t *state_id)
{
	unsigned int found_entry, i;
	int ret = PSCI_E_SUCCESS;
	const plat_state_prop_t *state_prop;

	assert(state_id);

	*state_id = 0;
	for (i = 0; i <= affinity_level; i++) {
		state_prop = plat_get_state_prop(i);
		if (!state_prop) {
			*state_id |= psci_make_local_state_id(i,
						PLAT_PSCI_DUMMY_STATE_ID);
			ret = PSCI_E_INVALID_PARAMS;
			continue;
		}
		found_entry = 0;

		while (state_prop->state_ID) {
			if (state_type == state_prop->is_pwrdown) {
				*state_id |= psci_make_local_state_id(i,
							state_prop->state_ID);
				found_entry = 1;
				break;
			}
			state_prop++;
		}
		if (!found_entry) {
			*state_id |= psci_make_local_state_id(i,
						PLAT_PSCI_DUMMY_STATE_ID);
			ret = PSCI_E_INVALID_PARAMS;
		}
	}

	return ret;
}

static unsigned int tftf_psci_get_pstate_format(void)
{
	int ret;

	ret = tftf_get_psci_feature_info(SMC_PSCI_CPU_SUSPEND);

	/*
	 * If error is returned, then it probably means that the PSCI version
	 * is less than 1.0 and only the original format is supported. In case
	 * the version is 1.0 or higher, then PSCI FEATURES which is a
	 * mandatory API is not implemented which implies that only the
	 * original format is supported.
	 */
	if (ret == PSCI_E_NOT_SUPPORTED)
		return CPU_SUSPEND_FEAT_PSTATE_FORMAT_ORIGINAL;

	/* Treat the invalid return value as PSCI FEATURES not supported */
	if ((ret & ~CPU_SUSPEND_FEAT_VALID_MASK) != 0)
		return CPU_SUSPEND_FEAT_PSTATE_FORMAT_ORIGINAL;

	return (ret >> CPU_SUSPEND_FEAT_PSTATE_FORMAT_SHIFT) & 0x1;
}

/* Make the power state in the original format */
uint32_t tftf_make_psci_pstate(uint32_t affinity_level,
					uint32_t state_type,
					uint32_t state_id)
{
	uint32_t power_state;

	assert(psci_state_type_valid(state_type));

	assert(pstate_format_detected);

	if (pstate_format == CPU_SUSPEND_FEAT_PSTATE_FORMAT_EXTENDED) {
		assert(psci_state_id_ext_valid(state_id));
		power_state = (state_type << PSTATE_TYPE_SHIFT_EXT)
					  | (state_id << PSTATE_ID_SHIFT_EXT);
	} else {
		assert(psci_affinity_level_valid(affinity_level));
		assert(psci_state_id_valid(state_id));
		power_state = (affinity_level << PSTATE_AFF_LVL_SHIFT)
					  | (state_type << PSTATE_TYPE_SHIFT);
		if (!is_state_id_null)
			power_state |= (state_id << PSTATE_ID_SHIFT);
	}

	return power_state;
}


void tftf_detect_psci_pstate_format(void)
{
	uint32_t power_state;
	unsigned int ret;

	pstate_format = tftf_psci_get_pstate_format();

	/*
	 * If the power state format is extended format, then the state-ID
	 * must use recommended encoding.
	 */
	if (pstate_format == CPU_SUSPEND_FEAT_PSTATE_FORMAT_EXTENDED) {
		pstate_format_detected = 1;
		INFO("Extended PSCI power state format detected\n");
		return;
	}

	tftf_irq_enable(IRQ_NS_SGI_0, GIC_HIGHEST_NS_PRIORITY);

	/*
	 * Mask IRQ to prevent the interrupt handler being invoked
	 * and clearing the interrupt. A pending interrupt will cause this
	 * CPU to wake-up from suspend.
	 */
	disable_irq();

	/* Configure an SGI to wake-up from suspend  */
	tftf_send_sgi(IRQ_NS_SGI_0,
		platform_get_core_pos(read_mpidr_el1() & MPID_MASK));


	/*
	 * Try to detect if the platform uses NULL State-ID encoding by sending
	 * PSCI_SUSPEND call with the NULL State-ID encoding. If the call
	 * succeeds then the platform uses NULL State-ID encoding. Else it
	 * uses the recommended encoding for State-ID.
	 */
	power_state = (PSTATE_AFF_LVL_0 << PSTATE_AFF_LVL_SHIFT)
		  | (PSTATE_TYPE_STANDBY << PSTATE_TYPE_SHIFT);

	ret = tftf_cpu_suspend(power_state);

	/* Unmask the IRQ to let the interrupt handler to execute */
	enable_irq();
	isb();

	tftf_irq_disable(IRQ_NS_SGI_0);

	/*
	 * The NULL State-ID returned SUCCESS. Hence State-ID is NULL
	 * for the power state format.
	 */
	if (ret == PSCI_E_SUCCESS) {
		is_state_id_null = 1;
		INFO("Original PSCI power state format with NULL State-ID detected\n");
	} else
		INFO("Original PSCI power state format detected\n");


	pstate_format_detected = 1;
}

unsigned int tftf_is_psci_state_id_null(void)
{
	assert(pstate_format_detected);

	if (pstate_format == CPU_SUSPEND_FEAT_PSTATE_FORMAT_ORIGINAL)
		return is_state_id_null;
	else
		return 0; /* Extended state ID does not support null format */
}

unsigned int tftf_is_psci_pstate_format_original(void)
{
	assert(pstate_format_detected);

	return pstate_format == CPU_SUSPEND_FEAT_PSTATE_FORMAT_ORIGINAL;
}

unsigned int tftf_get_psci_version(void)
{
	smc_args args = { SMC_PSCI_VERSION };
	smc_ret_values ret;

	ret = tftf_smc(&args);

	return ret.ret0;
}

int tftf_is_valid_psci_version(unsigned int version)
{
	if (version != PSCI_VERSION(1, 1) &&
	    version != PSCI_VERSION(1, 0) &&
	    version != PSCI_VERSION(0, 2) &&
	    version != PSCI_VERSION(0, 1)) {
		return 0;
	}
	return 1;
}
