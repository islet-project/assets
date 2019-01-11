/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>
#include <cactus_def.h>
#include <debug.h>
#include <drivers/arm/pl011.h>
#include <drivers/console.h>
#include <errno.h>
#include <plat_arm.h>
#include <platform_def.h>
#include <sp_helpers.h>
#include <sprt_client.h>
#include <sprt_svc.h>
#include <std_svc.h>

#include "cactus.h"
#include "cactus_def.h"
#include "cactus_tests.h"

/* Host machine information injected by the build system in the ELF file. */
extern const char build_message[];
extern const char version_string[];

static void cactus_print_memory_layout(void)
{
	NOTICE("Secure Partition memory layout:\n");

	NOTICE("  Image regions\n");
	NOTICE("    Text region            : %p - %p\n",
		(void *)CACTUS_TEXT_START, (void *)CACTUS_TEXT_END);
	NOTICE("    Read-only data region  : %p - %p\n",
		(void *)CACTUS_RODATA_START, (void *)CACTUS_RODATA_END);
	NOTICE("    Data region            : %p - %p\n",
		(void *)CACTUS_DATA_START, (void *)CACTUS_DATA_END);
	NOTICE("    BSS region             : %p - %p\n",
		(void *)CACTUS_BSS_START, (void *)CACTUS_BSS_END);
	NOTICE("    Total image memory     : %p - %p\n",
		(void *)CACTUS_IMAGE_BASE,
		(void *)(CACTUS_IMAGE_BASE + CACTUS_IMAGE_SIZE));
	NOTICE("  SPM regions\n");
	NOTICE("    SPM <-> SP buffer      : %p - %p\n",
		(void *)CACTUS_SPM_BUF_BASE,
		(void *)(CACTUS_SPM_BUF_BASE + CACTUS_SPM_BUF_SIZE));
	NOTICE("    NS <-> SP buffer       : %p - %p\n",
		(void *)CACTUS_NS_BUF_BASE,
		(void *)(CACTUS_NS_BUF_BASE + CACTUS_NS_BUF_SIZE));
	NOTICE("  Test regions\n");
	NOTICE("    Test region            : %p - %p\n",
		(void *)CACTUS_TEST_MEM_BASE,
		(void *)(CACTUS_TEST_MEM_BASE + CACTUS_TEST_MEM_SIZE));
}

static void cactus_message_handler(struct sprt_queue_entry_message *message)
{
	u_register_t ret0 = 0U, ret1 = 0U, ret2 = 0U, ret3 = 0U;

	if (message->type == SPRT_MSG_TYPE_SERVICE_REQUEST) {
		switch (message->args[1]) {

		case CACTUS_PRINT_MAGIC:
			INFO("Cactus: Magic: 0x%x\n", CACTUS_MAGIC_NUMBER);
			ret0 = SPRT_SUCCESS;
			break;

		case CACTUS_GET_MAGIC:
			ret1 = CACTUS_MAGIC_NUMBER;
			ret0 = SPRT_SUCCESS;
			break;

		case CACTUS_SLEEP_MS:
			sp_sleep(message->args[2]);
			ret0 = SPRT_SUCCESS;
			break;

		default:
			NOTICE("Cactus: Unhandled Service ID 0x%x\n",
			       (unsigned int)message->args[1]);
			ret0 = SPRT_NOT_SUPPORTED;
			break;
		}
	} else {
		NOTICE("Cactus: Unhandled Service type 0x%x\n",
		       (unsigned int)message->type);
		ret0 = SPRT_NOT_SUPPORTED;
	}

	sprt_message_end(message, ret0, ret1, ret2, ret3);
}

void __dead2 cactus_main(void)
{
	console_init(PL011_UART2_BASE,
		     PL011_UART2_CLK_IN_HZ,
		     PL011_BAUDRATE);

	NOTICE("Booting test Secure Partition Cactus\n");
	NOTICE("%s\n", build_message);
	NOTICE("%s\n", version_string);
	NOTICE("Running at S-EL0\n");

	cactus_print_memory_layout();

	/*
	 * Run some initial tests.
	 *
	 * These are executed when the system is still booting, just after SPM
	 * has handed over to Cactus.
	 */
	misc_tests();
	system_setup_tests();
	mem_attr_changes_tests();

	/*
	 * Handle secure service requests.
	 */
	sprt_initialize_queues((void *)CACTUS_SPM_BUF_BASE);

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
					cactus_message_handler(&message);
				}
			} else {
				assert(err == 0);
				cactus_message_handler(&message);
			}
		}

		sprt_wait_for_messages();
	}
}
