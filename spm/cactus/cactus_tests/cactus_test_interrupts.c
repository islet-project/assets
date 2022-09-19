/*
 * Copyright (c) 2021-2022, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <common/debug.h>
#include <drivers/arm/sp805.h>
#include <sp_helpers.h>
#include <spm_helpers.h>

#include "cactus_message_loop.h"
#include "cactus_test_cmds.h"

#include <platform.h>

/* Secure virtual interrupt that was last handled by Cactus SP. */
extern uint32_t last_serviced_interrupt[PLATFORM_CORE_COUNT];
static int flag_set;

static void sec_wdog_interrupt_handled(void)
{
	expect(flag_set, 0);
	flag_set = 1;
}

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

CACTUS_CMD_HANDLER(sleep_fwd_cmd, CACTUS_FWD_SLEEP_CMD)
{
	struct ffa_value ffa_ret;
	ffa_id_t vm_id = ffa_dir_msg_dest(*args);
	ffa_id_t fwd_dest = cactus_get_fwd_sleep_dest(*args);
	uint32_t sleep_ms = cactus_get_sleep_time(*args);
	bool hint_interrupted = cactus_get_fwd_sleep_interrupted_hint(*args);
	bool fwd_dest_interrupted;

	VERBOSE("VM%x requested %x to sleep for value %u\n",
		ffa_dir_msg_source(*args), fwd_dest, sleep_ms);

	ffa_ret = cactus_sleep_cmd(vm_id, fwd_dest, sleep_ms);

	/*
	 * The target of the direct request could be pre-empted any number of
	 * times. Moreover, the target SP may or may not support managed exit.
	 * Hence, the target is allocated cpu cycles in this while loop.
	 */
	while ((ffa_func_id(ffa_ret) == FFA_INTERRUPT) ||
		is_expected_cactus_response(ffa_ret, MANAGED_EXIT_INTERRUPT_ID,
					    0)) {
		fwd_dest_interrupted = true;

		if (ffa_func_id(ffa_ret) == FFA_INTERRUPT) {
			/* Received FFA_INTERRUPT in blocked state. */
			VERBOSE("Processing FFA_INTERRUPT while"
				" blocked on direct response\n");
			unsigned int my_core_pos =
				platform_get_core_pos(read_mpidr_el1());

			ffa_ret = ffa_run(fwd_dest, my_core_pos);
		} else {
			/*
			 * Destination sent managed exit response. Allocate
			 * dummy cycles through direct request message to
			 * destination SP.
			 */
			VERBOSE("SP%x: received Managed Exit as response\n",
				vm_id);
			ffa_ret = cactus_resume_after_managed_exit(vm_id,
								   fwd_dest);
		}
	}

	if (hint_interrupted && !fwd_dest_interrupted) {
		ERROR("Forwaded destination of the sleep command was not"
		      " interrupted as anticipated\n");
		return cactus_error_resp(vm_id, ffa_dir_msg_source(*args),
					 CACTUS_ERROR_TEST);
	}

	if (!is_ffa_direct_response(ffa_ret)) {
		ERROR("Encountered error in CACTUS_FWD_SLEEP_CMD response\n");
		return cactus_error_resp(vm_id, ffa_dir_msg_source(*args),
					 CACTUS_ERROR_FFA_CALL);
	}

	if (cactus_get_response(ffa_ret) < sleep_ms) {
		ERROR("Request returned: %u ms!\n",
		      cactus_get_response(ffa_ret));
		return cactus_error_resp(vm_id, ffa_dir_msg_source(*args),
					 CACTUS_ERROR_TEST);

	}

	return cactus_success_resp(vm_id, ffa_dir_msg_source(*args), 0);
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

bool handle_twdog_interrupt_sp_sleep(uint32_t sleep_time, uint64_t *time_lapsed)
{
	sp_register_interrupt_tail_end_handler(sec_wdog_interrupt_handled,
						IRQ_TWDOG_INTID);
	*time_lapsed += sp_sleep_elapsed_time(sleep_time);

	if (flag_set == 0) {
		return false;
	}

	/* Reset the flag and unregister the handler. */
	flag_set = 0;
	sp_unregister_interrupt_tail_end_handler(IRQ_TWDOG_INTID);

	return true;
}

CACTUS_CMD_HANDLER(sleep_twdog_cmd, CACTUS_SLEEP_TRIGGER_TWDOG_CMD)
{
	uint64_t time_lapsed = 0;
	uint32_t sleep_time = cactus_get_sleep_time(*args) / 2;
	uint64_t time_ms = cactus_get_wdog_trigger_duration(*args);

	VERBOSE("Request to sleep %x for %ums.\n", ffa_dir_msg_dest(*args),
		sleep_time);

	if (!handle_twdog_interrupt_sp_sleep(sleep_time, &time_lapsed)) {
		goto fail;
	}

	/* Lapsed time should be at least equal to sleep time. */
	VERBOSE("Sleep complete: %llu\n", time_lapsed);

	VERBOSE("Starting TWDOG: %llums\n", time_ms);
	sp805_twdog_refresh();
	sp805_twdog_start((time_ms * ARM_SP805_TWDG_CLK_HZ) / 1000);

	VERBOSE("2nd Request to sleep %x for %ums.\n", ffa_dir_msg_dest(*args),
		sleep_time);

	if (!handle_twdog_interrupt_sp_sleep(sleep_time, &time_lapsed)) {
		goto fail;
	}

	/* Lapsed time should be at least equal to sleep time. */
	VERBOSE("2nd Sleep complete: %llu\n", time_lapsed);

	return cactus_response(ffa_dir_msg_dest(*args),
			       ffa_dir_msg_source(*args),
			       time_lapsed);
fail:
	/* Test failed. */
	ERROR("Watchdog interrupt not handled\n");
	return cactus_error_resp(ffa_dir_msg_dest(*args),
				 ffa_dir_msg_source(*args),
				 CACTUS_ERROR_TEST);
}

CACTUS_CMD_HANDLER(interrupt_serviced_cmd, CACTUS_LAST_INTERRUPT_SERVICED_CMD)
{
	unsigned int core_pos = get_current_core_id();

	return cactus_response(ffa_dir_msg_dest(*args),
			       ffa_dir_msg_source(*args),
			       last_serviced_interrupt[core_pos]);
}
