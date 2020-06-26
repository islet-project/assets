/*
 * Copyright (c) 2020, NVIDIA Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <arch.h>
#include <platform.h>
#include <stddef.h>

#include <psci.h>
#include <utils_def.h>

/*
 * State IDs for local power states
 */
#define TEGRA210_RUN_STATE_ID		U(0) /* Valid for CPUs and Clusters */
#define TEGRA210_CORE_RETN_STATE_ID	U(6) /* Valid for only CPUs */
#define TEGRA210_CORE_OFF_STATE_ID	U(7) /* Valid for CPUs and Clusters */
#define TEGRA210_SOC_OFF_STATE_ID	U(2) /* Valid for the System */

/*
 * Suspend depth definitions for each power state
 */
typedef enum {
	TEGRA210_RUN_DEPTH = 0,
	TEGRA210_CORE_RETENTION_DEPTH,
	TEGRA210_CORE_OFF_DEPTH,
	TEGRA210_SYSTEM_OFF_DEPTH
} suspend_depth_t;

/* The state property array with details of idle state possible for the core */
static const plat_state_prop_t core_state_prop[] = {
	{TEGRA210_CORE_RETENTION_DEPTH, TEGRA210_CORE_RETN_STATE_ID, PSTATE_TYPE_STANDBY},
	{TEGRA210_CORE_OFF_DEPTH, TEGRA210_CORE_OFF_STATE_ID, PSTATE_TYPE_POWERDOWN},
	{0}
};

/*
 * The state property array with details of idle state possible
 * for the cluster
 */
static const plat_state_prop_t cluster_state_prop[] = {
	{TEGRA210_CORE_OFF_DEPTH, TEGRA210_CORE_OFF_STATE_ID, PSTATE_TYPE_POWERDOWN},
	{0}
};

/*
 * The state property array with details of idle state possible
 * for the system. Currently Tegra210 does not support CPU SUSPEND
 * at system power level.
 */
static const plat_state_prop_t system_state_prop[] = {
	{TEGRA210_SYSTEM_OFF_DEPTH, TEGRA210_SOC_OFF_STATE_ID, PSTATE_TYPE_POWERDOWN},
	{0}
};

/*
 * This functions returns the plat_state_prop_t array for all the valid low
 * power states from platform for a specified affinity level and returns NULL
 * for an invalid affinity level. The array is expected to be NULL-terminated.
 * This function is expected to be used by tests that need to compose the power
 * state parameter for use in PSCI_CPU_SUSPEND API or PSCI_STAT/RESIDENCY
 * API.
 */
const plat_state_prop_t *plat_get_state_prop(unsigned int level)
{
	switch (level) {
	case MPIDR_AFFLVL0:
		return core_state_prop;
	case MPIDR_AFFLVL1:
		return cluster_state_prop;
	case MPIDR_AFFLVL2:
		return system_state_prop;
	default:
		return NULL;
	}
}
