/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <platform.h>
#include <psci.h>

/* State IDs for local power states on SGI platforms. */
#define SGI_PS_RUN_STATE_ID		0 /* Valid for CPUs and Clusters */
#define SGI_PS_RETENTION_STATE_ID	1 /* Valid for only CPUs */
#define SGI_PS_OFF_STATE_ID		2 /* Valid for CPUs and Clusters */

/* Suspend depth definitions for each power state */
#define SGI_PS_RUN_DEPTH	0
#define SGI_PS_RETENTION_DEPTH	1
#define SGI_PS_OFF_DEPTH	2

/* The state property array with details of idle state possible for the core */
static const plat_state_prop_t core_state_prop[] = {
	{SGI_PS_RETENTION_DEPTH, SGI_PS_RETENTION_STATE_ID,
		PSTATE_TYPE_STANDBY},
	{SGI_PS_OFF_DEPTH, SGI_PS_OFF_STATE_ID, PSTATE_TYPE_POWERDOWN},
	{0},
};

/* The state property array with details of idle state possible for the cluster */
static const plat_state_prop_t cluster_state_prop[] = {
	{SGI_PS_OFF_DEPTH, SGI_PS_OFF_STATE_ID, PSTATE_TYPE_POWERDOWN},
	{0},
};

const plat_state_prop_t *plat_get_state_prop(unsigned int level)
{
	switch (level) {
	case MPIDR_AFFLVL0:
		return core_state_prop;
	case MPIDR_AFFLVL1:
		return cluster_state_prop;
	default:
		return NULL;
	}
}
