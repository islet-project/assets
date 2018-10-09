/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <debug.h>
#include <platform.h>
#include <platform_def.h>
#include <psci.h>
#include <tftf.h>

static unsigned int pstate_initialized;

/*
 * Stores pointer to the state_prop_t for all implemented levels
 */
static const plat_state_prop_t *plat_state_ptr[PLAT_MAX_PWR_LEVEL + 1];

/*
 * Saves number of implemented power states per level
 */
static unsigned int power_states_per_level[PLAT_MAX_PWR_LEVEL + 1];

void tftf_init_pstate_framework(void)
{
	int i, j;

	if (pstate_initialized)
		return;

	/* Detect the PSCI power state format used. */
	tftf_detect_psci_pstate_format();

	/*
	 * Get and save the pointers to plat_state_prop_t values for all
	 * levels. Also, store the max number of local states possible for
	 * each level in power_states_per_level.
	 */
	for (i = 0; i <= PLAT_MAX_PWR_LEVEL; i++) {
		plat_state_ptr[i] = plat_get_state_prop(i);
		assert(plat_state_ptr[i]);

		for (j = 0; (plat_state_ptr[i]+j)->state_ID != 0; j++)
			;

		power_states_per_level[i] = j;
	}

	pstate_initialized = 1;
}

void tftf_set_next_state_id_idx(unsigned int power_level,
					unsigned int pstate_id_idx[])
{
	unsigned int i;
#if ENABLE_ASSERTIONS
	/* Verify that this is a valid power level. */
	assert(power_level <= PLAT_MAX_PWR_LEVEL);

	/*
	 * Verify if a level has PWR_STATE_INIT_INDEX index, all higher levels
	 * have to be in PWR_STATE_INIT_INDEX. Not needed to check the top
	 * power level in the outer loop.
	 */
	for (i = 0; i < power_level; i++) {
		if (pstate_id_idx[i] == PWR_STATE_INIT_INDEX) {
			for ( ; i <= power_level; i++)
				assert(pstate_id_idx[i] == PWR_STATE_INIT_INDEX);
		}
	}
#endif

	/* Increment the pstate_id_idx starting from the lowest power level */
	for (i = 0; i <= power_level; i++) {
		pstate_id_idx[i]++;

		/*
		 * Wraparound index if the maximum power states available for
		 * that level is reached and proceed to next level.
		 */
		if (pstate_id_idx[i] == power_states_per_level[i])
			pstate_id_idx[i] = 0;
		else
			break;
	}

	/*
	 * Check if the requested power level has wrapped around. If it has,
	 * reset pstate_id_idx.
	 */
	if (i > power_level) {
		for (i = 0; i <= power_level; i++)
			pstate_id_idx[i] = PWR_STATE_INIT_INDEX;
	}
}

void tftf_set_deepest_pstate_idx(unsigned int power_level,
				unsigned int pstate_id_idx[])
{
	int i;

	/* Verify that this is a valid power level. */
	assert(power_level <= PLAT_MAX_PWR_LEVEL);

	/*
	 * Assign the highest pstate_id_idx starting from the lowest power
	 * level
	 */
	for (i = 0; i <= power_level; i++)
		pstate_id_idx[i] = power_states_per_level[i] - 1;
}


int tftf_get_pstate_vars(unsigned int *test_power_level,
				unsigned int *test_suspend_type,
				unsigned int *suspend_state_id,
				unsigned int pstate_id_idx[])
{
	unsigned int i;
	int state_id = 0;
	int suspend_type;
	int suspend_depth;
	int psci_ret = PSCI_E_SUCCESS;
	const plat_state_prop_t *local_state;

	/* Atleast one entry should be valid to generate correct power state params */
	assert(pstate_id_idx[0] != PWR_STATE_INIT_INDEX &&
			pstate_id_idx[0] <= power_states_per_level[0]);

	suspend_depth = (plat_state_ptr[0] + pstate_id_idx[0])->suspend_depth;
	suspend_type = (plat_state_ptr[0] + pstate_id_idx[0])->is_pwrdown;

	for (i = 0; i <= PLAT_MAX_PWR_LEVEL; i++) {

		/* Reached all levels with the valid power index values */
		if (pstate_id_idx[i] == PWR_STATE_INIT_INDEX)
			break;

		assert(pstate_id_idx[i] <= power_states_per_level[i]);

		local_state = plat_state_ptr[i] + pstate_id_idx[i];
		state_id |= (local_state->state_ID << i * PLAT_LOCAL_PSTATE_WIDTH);

		if (local_state->is_pwrdown > suspend_type)
			suspend_type = local_state->is_pwrdown;

		if (local_state->suspend_depth > suspend_depth)
			psci_ret = PSCI_E_INVALID_PARAMS;
		else
			suspend_depth = local_state->suspend_depth;
	}

	*test_suspend_type = suspend_type;
	*suspend_state_id = state_id;
	*test_power_level = --i;

	return psci_ret;
}

void tftf_set_next_local_state_id_idx(unsigned int power_level,
						unsigned int pstate_id_idx[])
{
	assert(power_level <= PLAT_MAX_PWR_LEVEL);

	if (pstate_id_idx[power_level] + 1 >= power_states_per_level[power_level]) {
		pstate_id_idx[power_level] = PWR_STATE_INIT_INDEX;
		return;
	}

	pstate_id_idx[power_level]++;
}
