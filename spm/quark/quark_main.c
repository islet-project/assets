/*
 * Copyright (c) 2018-2019, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>
#include <cdefs.h>
#include <errno.h>
#include <quark_def.h>
#include <sprt_client.h>
#include <sprt_svc.h>
#include <utils_def.h>

#include "quark.h"
#include "quark_def.h"

/* NOTE: This partition doesn't have text output capabilities */

static void quark_message_handler(struct sprt_queue_entry_message *message)
{
	u_register_t ret0 = 0U, ret1 = 0U, ret2 = 0U, ret3 = 0U;

	if (message->type == SPRT_MSG_TYPE_SERVICE_REQUEST) {
		switch (message->args[1]) {

		case QUARK_GET_MAGIC:
			ret1 = QUARK_MAGIC_NUMBER;
			ret0 = SPRT_SUCCESS;
			break;

		default:
			ret0 = SPRT_NOT_SUPPORTED;
			break;
		}
	} else {
		ret0 = SPRT_NOT_SUPPORTED;
	}

	sprt_message_end(message, ret0, ret1, ret2, ret3);
}

void __dead2 quark_main(void)
{
	/*
	 * Handle secure service requests.
	 */
	sprt_initialize_queues((void *)QUARK_SPM_BUF_BASE);

	while (1) {
		struct sprt_queue_entry_message message;

		/*
		 * Try to fetch a message from the blocking requests queue. If
		 * it is empty, try to fetch from the non-blocking requests
		 * queue. Repeat until both of them are empty.
		 */
		while (1) {
			int err = sprt_get_next_message(&message,
					SPRT_QUEUE_NUM_BLOCKING);
			if (err == -ENOENT) {
				err = sprt_get_next_message(&message,
						SPRT_QUEUE_NUM_NON_BLOCKING);
				if (err == -ENOENT) {
					break;
				} else {
					assert(err == 0);
					quark_message_handler(&message);
				}
			} else {
				assert(err == 0);
				quark_message_handler(&message);
			}
		}

		sprt_wait_for_messages();
	}
}
