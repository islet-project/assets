/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef SPRT_CLIENT_H
#define SPRT_CLIENT_H

#include <stdint.h>

#include "sprt_common.h"

/*
 * Point the SPRT library at a shared buffer between SPM and SP.
 */
void sprt_initialize_queues(void *buffer_base);

/*
 * Return SPRT version.
 */
uint32_t sprt_version(void);

/*
 * Called by the main SPRT client execution context when there are no more
 * messages available via sprt_get_next_message(), or if the SPRT client wishes
 * to yield execution to allow other SPs to run.
 */
void sprt_wait_for_messages(void);

/*
 * Returns the next message to be processed by the SPRT client. There can be
 * multiple queues of messages for a partition, `queue_num` specifies which
 * queue to read from. Each message can be retrieved only once. The message
 * pointer must point to a valid memory owned by the caller. A zero return
 * value indicates there is a message for the SP, -ENOENT means there are no
 * messages.
 */
int sprt_get_next_message(struct sprt_queue_entry_message *message,
			  int queue_num);

/*
 * End processing of the message passing arg0 to arg3 back to the SPCI client.
 */
void sprt_message_end(struct sprt_queue_entry_message *message,
		      u_register_t arg0, u_register_t arg1, u_register_t arg2,
		      u_register_t arg3);

#endif /* SPRT_CLIENT_H */
