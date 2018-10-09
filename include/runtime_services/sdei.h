/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __SDEI_H__
#define __SDEI_H__

#define SDEI_VERSION				0xC4000020
#define SDEI_EVENT_REGISTER			0xC4000021
#define SDEI_EVENT_ENABLE			0xC4000022
#define SDEI_EVENT_DISABLE			0xC4000023
#define SDEI_EVENT_CONTEXT			0xC4000024
#define SDEI_EVENT_COMPLETE			0xC4000025
#define SDEI_EVENT_COMPLETE_AND_RESUME		0xC4000026

#define SDEI_EVENT_UNREGISTER			0xC4000027
#define SDEI_EVENT_STATUS			0xC4000028
#define SDEI_EVENT_GET_INFO			0xC4000029
#define SDEI_EVENT_ROUTING_SET			0xC400002A
#define SDEI_PE_MASK				0xC400002B
#define SDEI_PE_UNMASK				0xC400002C

#define SDEI_INTERRUPT_BIND			0xC400002D
#define SDEI_INTERRUPT_RELEASE			0xC400002E
#define SDEI_EVENT_SIGNAL			0xC400002F
#define SDEI_FEATURES				0xC4000030
#define SDEI_PRIVATE_RESET			0xC4000031
#define SDEI_SHARED_RESET			0xC4000032

/* For debug */
#define SDEI_SHOW_DEBUG				0xC400003F

/* SDEI_EVENT_REGISTER flags */
#define SDEI_REGF_RM_ANY	0
#define SDEI_REGF_RM_PE		1

/* SDEI_EVENT_COMPLETE status flags */
#define SDEI_EV_HANDLED		0
#define SDEI_EV_FAILED		1

/* sde event status values in bit position */
#define SDEI_STATF_REGISTERED		0
#define SDEI_STATF_ENABLED		1
#define SDEI_STATF_RUNNING		2

#define SDEI_INFOF_TYPE			0
#define SDEI_INFOF_SIGNALABLE		1
#define SDEI_INFOF_ROUTING_MODE		2
#define SDEI_INFOF_ROUTING_AFF		3

#define	SMC_EINVAL	2
#define	SMC_EDENY	3
#define	SMC_EPEND	5
#define	SMC_ENOMEM	10

#define MAKE_SDEI_VERSION(_major, _minor, _vendor) \
	(((uint64_t)(_major)) << 48 | \
	((uint64_t)(_minor)) << 32 | \
	(_vendor))

#ifndef __ASSEMBLY__
#include <stdint.h>

struct sdei_intr_ctx {
	unsigned int priority;
	unsigned int num;
	unsigned int enabled;
};

typedef int sdei_handler_t(int ev, uint64_t arg);

void sdei_trigger_event(void);
void sdei_handler_done(void);

int64_t sdei_version(void);
int64_t sdei_interrupt_bind(int intr, struct sdei_intr_ctx *intr_ctx);
int64_t sdei_interrupt_release(int intr, const struct sdei_intr_ctx *intr_ctx);
int64_t sdei_event_register(int ev, sdei_handler_t *ep,
	uint64_t ep_arg, int flags, uint64_t mpidr);
int64_t sdei_event_unregister(int ev);
int64_t sdei_event_enable(int ev);
int64_t sdei_event_disable(int ev);
int64_t sdei_pe_mask(void);
int64_t sdei_pe_unmask(void);
int64_t sdei_private_reset(void);
int64_t sdei_shared_reset(void);
int64_t sdei_event_signal(uint64_t mpidr);
int64_t sdei_event_status(int32_t ev);
int64_t sdei_event_routing_set(int32_t ev, uint64_t flags);
int64_t sdei_event_context(uint32_t param);
int64_t sdei_event_complete(uint32_t flags);
int64_t sdei_event_complete_and_resume(uint64_t addr);
#endif /* __ASSEMBLY__ */

#endif /* __SDEI_H__ */
