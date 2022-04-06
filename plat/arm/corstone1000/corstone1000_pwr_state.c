/*
 * Copyright (c) 2022, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stddef.h>
#include <arch.h>
#include <platform.h>
#include <psci.h>

/*
 * State IDs for local power states on Corstone1000.
 */
#define CORSTONE1000_RUN_STATE_ID		0 /* Valid for CPUs and Clusters */
#define CORSTONE1000_RETENTION_STATE_ID		1 /* Valid for only CPUs */
#define CORSTONE1000_OFF_STATE_ID		2 /* Valid for CPUs and Clusters */

/*
 * Suspend depth definitions for each power state
 */
typedef enum {
	CORSTONE1000_RUN_DEPTH = 0,
	CORSTONE1000_RETENTION_DEPTH,
	CORSTONE1000_OFF_DEPTH,
} suspend_depth_t;

/* The state property array with details of idle state possible for the core */
static const plat_state_prop_t core_state_prop[] = {
	{CORSTONE1000_RETENTION_DEPTH, CORSTONE1000_RETENTION_STATE_ID, PSTATE_TYPE_STANDBY},
	{CORSTONE1000_OFF_DEPTH, CORSTONE1000_OFF_STATE_ID, PSTATE_TYPE_POWERDOWN},
	{0},
};

/*
 * The state property array with details of idle state possible
 * for the cluster
 */
static const plat_state_prop_t cluster_state_prop[] = {
	{CORSTONE1000_OFF_DEPTH, CORSTONE1000_OFF_STATE_ID, PSTATE_TYPE_POWERDOWN},
	{0},
};

/*
 * The state property array with details of idle state possible
 * for the system. Currently Corstone1000 does not support CPU SUSPEND
 * at system power level.
 */
static const plat_state_prop_t system_state_prop[] = {
	{CORSTONE1000_OFF_DEPTH, CORSTONE1000_OFF_STATE_ID, PSTATE_TYPE_POWERDOWN},
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
