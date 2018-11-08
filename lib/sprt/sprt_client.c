/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>
#include <errno.h>
#include <sprt_svc.h>
#include <stddef.h>

#include "sprt_client.h"
#include "sprt_client_private.h"
#include "sprt_common.h"
#include "sprt_queue.h"

uint32_t sprt_version(void)
{
	struct svc_args args;

	args.arg0 = SPRT_VERSION;

	return sprt_client_svc(&args);
}

void sprt_wait_for_messages(void)
{
	struct svc_args args;

	args.arg0 = SPRT_YIELD_AARCH64;

	sprt_client_svc(&args);
}

/*
 * Variable that points to the memory buffer that contains the queues used by
 * this Secure Partition.
 */
static void *queue_messages;

void sprt_initialize_queues(void *buffer_base)
{
	queue_messages = buffer_base;
}

int sprt_get_next_message(struct sprt_queue_entry_message *message,
			  int queue_num)
{
	struct sprt_queue *q = queue_messages;

	while (queue_num-- > 0) {
		uintptr_t next_addr = (uintptr_t)q + sizeof(struct sprt_queue) +
				      q->entry_num * q->entry_size;
		q = (struct sprt_queue *) next_addr;
	}

	return sprt_queue_pop(q, message);
}

void sprt_message_end(struct sprt_queue_entry_message *message,
		      u_register_t arg0, u_register_t arg1, u_register_t arg2,
		      u_register_t arg3)
{
	struct svc_args args;

	if (message->type == SPRT_MSG_TYPE_SERVICE_REQUEST) {
		args.arg0 = SPRT_PUT_RESPONSE_AARCH64;
		args.arg1 = message->token;
	}

	args.arg2 = arg0;
	args.arg3 = arg1;
	args.arg4 = arg2;
	args.arg5 = arg3;
	args.arg6 = ((uint32_t)message->service_handle << 16U)
		  | message->client_id;
	args.arg7 = message->session_id;

	sprt_client_svc(&args);
}
