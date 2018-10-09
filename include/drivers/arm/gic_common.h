/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __GIC_COMMON_H__
#define __GIC_COMMON_H__

/***************************************************************************
 * Defines and prototypes common to GIC v2 and v3 drivers.
 **************************************************************************/
/* Distributor interface register offsets */
#define GICD_CTLR		0x0
#define GICD_TYPER		0x4
#define GICD_ISENABLER		0x100
#define GICD_ICENABLER		0x180
#define GICD_ISPENDR		0x200
#define GICD_ICPENDR		0x280
#define GICD_ISACTIVER		0x300
#define GICD_ICACTIVER		0x380
#define GICD_IPRIORITYR		0x400
#define GICD_ICFGR		0xC00

/* Distributor interface register shifts */
#define ISENABLER_SHIFT		5
#define ICENABLER_SHIFT		ISENABLER_SHIFT
#define ISPENDR_SHIFT		5
#define ICPENDR_SHIFT		ISPENDR_SHIFT
#define ISACTIVER_SHIFT		5
#define ICACTIVER_SHIFT		ISACTIVER_SHIFT
#define IPRIORITYR_SHIFT	2
#define ICFGR_SHIFT		4

/* GICD_TYPER bit definitions */
#define IT_LINES_NO_MASK	0x1f

/* GICD Priority register mask */
#define GIC_PRI_MASK		0xff

/*
 * Number of per-cpu interrupts to save prior to system suspend.
 * This comprises all SGIs and PPIs.
 */
#define NUM_PCPU_INTR	32

#ifndef __ASSEMBLY__

#include <mmio.h>

/* Helper to detect the GIC mode (GICv2 or GICv3) configured in the system */
unsigned int is_gicv3_mode(void);

/*******************************************************************************
 * Private GIC Distributor function prototypes for use by GIC drivers
 ******************************************************************************/
unsigned int gicd_read_isenabler(unsigned int base, unsigned int interrupt_id);
unsigned int gicd_read_icenabler(unsigned int base, unsigned int interrupt_id);
unsigned int gicd_read_ispendr(unsigned int base, unsigned int interrupt_id);
unsigned int gicd_read_icpendr(unsigned int base, unsigned int interrupt_id);
unsigned int gicd_read_isactiver(unsigned int base, unsigned int interrupt_id);
unsigned int gicd_read_icactiver(unsigned int base, unsigned int interrupt_id);
unsigned int gicd_read_ipriorityr(unsigned int base, unsigned int interrupt_id);
unsigned int gicd_get_ipriorityr(unsigned int base, unsigned int interrupt_id);
unsigned int gicd_read_icfgr(unsigned int base, unsigned int interrupt_id);
void gicd_write_isenabler(unsigned int base, unsigned int interrupt_id,
					unsigned int val);
void gicd_write_icenabler(unsigned int base, unsigned int interrupt_id,
					unsigned int val);
void gicd_write_ispendr(unsigned int base, unsigned int interrupt_id,
					unsigned int val);
void gicd_write_icpendr(unsigned int base, unsigned int interrupt_id,
					unsigned int val);
void gicd_write_isactiver(unsigned int base, unsigned int interrupt_id,
					unsigned int val);
void gicd_write_icactiver(unsigned int base, unsigned int interrupt_id,
					unsigned int val);
void gicd_write_ipriorityr(unsigned int base, unsigned int interrupt_id,
					unsigned int val);
void gicd_write_icfgr(unsigned int base, unsigned int interrupt_id,
					unsigned int val);
unsigned int gicd_get_isenabler(unsigned int base, unsigned int interrupt_id);
void gicd_set_isenabler(unsigned int base, unsigned int interrupt_id);
void gicd_set_icenabler(unsigned int base, unsigned int interrupt_id);
void gicd_set_ispendr(unsigned int base, unsigned int interrupt_id);
void gicd_set_icpendr(unsigned int base, unsigned int interrupt_id);
void gicd_set_isactiver(unsigned int base, unsigned int interrupt_id);
void gicd_set_icactiver(unsigned int base, unsigned int interrupt_id);
void gicd_set_ipriorityr(unsigned int base, unsigned int interrupt_id,
					unsigned int priority);

/*******************************************************************************
 * Private GIC Distributor interface accessors for reading and writing
 * entire registers
 ******************************************************************************/
static inline unsigned int gicd_read_ctlr(unsigned int base)
{
	return mmio_read_32(base + GICD_CTLR);
}

static inline unsigned int gicd_read_typer(unsigned int base)
{
	return mmio_read_32(base + GICD_TYPER);
}

static inline void gicd_write_ctlr(unsigned int base, unsigned int val)
{
	mmio_write_32(base + GICD_CTLR, val);
}


#endif /*__ASSEMBLY__*/
#endif /* __GIC_COMMON_H__ */
