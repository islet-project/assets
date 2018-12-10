/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <arch.h>
#include <platform.h>
#include <psci.h>
#include <stddef.h>

/*
 * State IDs for local power states on the FVP.
 */
#define FVP_RUN_STATE_ID	0	/* Valid for CPUs and Clusters */
#define FVP_RETENTION_STATE_ID	1	/* Valid for only CPUs */
#define FVP_OFF_STATE_ID	2	/* Valid for CPUs and Clusters */

/*
 * Suspend depth definitions for each power state
 */
typedef enum {
	FVP_RUN_DEPTH = 0,
	FVP_RETENTION_DEPTH,
	FVP_OFF_DEPTH,
} suspend_depth_t;

/* The state property array with details of idle state possible for the core */
static const plat_state_prop_t core_state_prop[] = {
	{FVP_RETENTION_DEPTH, FVP_RETENTION_STATE_ID, PSTATE_TYPE_STANDBY},
	{FVP_OFF_DEPTH, FVP_OFF_STATE_ID, PSTATE_TYPE_POWERDOWN},
	{0},
};

/* The state property array with details of idle state possible
   for the cluster */
static const plat_state_prop_t cluster_state_prop[] = {
	{FVP_OFF_DEPTH, FVP_OFF_STATE_ID, PSTATE_TYPE_POWERDOWN},
	{0},
};

/* The state property array with details of idle state possible
   for the system level */
static const plat_state_prop_t system_state_prop[] = {
	{FVP_OFF_DEPTH, FVP_OFF_STATE_ID, PSTATE_TYPE_POWERDOWN},
	{0},
};

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
