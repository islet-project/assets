/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __ARCH_H__
#define __ARCH_H__

#define MPIDR_MT_MASK		(1 << 24)
#define MPIDR_AFFLVL_MASK	0xff
#define MPIDR_AFFINITY_BITS	8
#define MPIDR_CPU_MASK		MPIDR_AFFLVL_MASK
#define MPIDR_CLUSTER_MASK	(MPIDR_AFFLVL_MASK << MPIDR_AFFINITY_BITS)
#define MPIDR_AFF0_SHIFT	0
#define MPIDR_AFF1_SHIFT	8
#define MPIDR_AFF2_SHIFT	16
#define MPIDR_AFFINITY_MASK	0xff00ffffff

#endif /* __ARCH_H__ */
