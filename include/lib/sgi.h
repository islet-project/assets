/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __SGI_H__
#define __SGI_H__

/* Data associated with the reception of an SGI */
typedef struct {
	/* Interrupt ID of the signaled interrupt */
	unsigned int irq_id;
} sgi_data_t;

/*
 * Send an SGI to a given core.
 */
void tftf_send_sgi(unsigned int sgi_id, unsigned int core_pos);

#endif /* __SGI_H__ */
