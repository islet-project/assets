/*
 * Copyright (c) 2021, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <common/debug.h>
#include <drivers/arm/sp805.h>
#include <sp_helpers.h>
#include <spm_helpers.h>

#include "cactus_message_loop.h"
#include "cactus_test_cmds.h"

CACTUS_CMD_HANDLER(sleep_cmd, CACTUS_SLEEP_CMD)
{
	uint64_t time_lapsed;
	uint32_t sleep_time = cactus_get_sleep_time(*args);

	VERBOSE("Request to sleep %x for %ums.\n", ffa_dir_msg_dest(*args),
		sleep_time);

	time_lapsed = sp_sleep_elapsed_time(sleep_time);

	/* Lapsed time should be at least equal to sleep time. */
	VERBOSE("Sleep complete: %llu\n", time_lapsed);

	return cactus_response(ffa_dir_msg_dest(*args),
			       ffa_dir_msg_source(*args),
			       time_lapsed);
}

CACTUS_CMD_HANDLER(interrupt_cmd, CACTUS_INTERRUPT_CMD)
{
	uint32_t int_id = cactus_get_interrupt_id(*args);
	bool enable = cactus_get_interrupt_enable(*args);
	enum interrupt_pin pin = cactus_get_interrupt_pin(*args);
	int64_t ret;

	ret = spm_interrupt_enable(int_id, enable, pin);
	if (ret != 0) {
		return cactus_error_resp(ffa_dir_msg_dest(*args),
					 ffa_dir_msg_source(*args),
					 CACTUS_ERROR_TEST);
	}

	return cactus_response(ffa_dir_msg_dest(*args),
			       ffa_dir_msg_source(*args),
			       CACTUS_SUCCESS);
}

CACTUS_CMD_HANDLER(twdog_cmd, CACTUS_TWDOG_START_CMD)
{
	ffa_id_t vm_id = ffa_dir_msg_dest(*args);
	ffa_id_t source = ffa_dir_msg_source(*args);

	uint64_t time_ms = cactus_get_wdog_duration(*args);

	VERBOSE("Starting TWDOG: %llums\n", time_ms);
	sp805_twdog_refresh();
	sp805_twdog_start((time_ms * ARM_SP805_TWDG_CLK_HZ) / 1000);

	return cactus_success_resp(vm_id, source, time_ms);
}
