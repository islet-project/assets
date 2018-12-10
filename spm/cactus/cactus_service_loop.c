/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>
#include <debug.h>
#include <sp_helpers.h>
#include <sprt_svc.h>
#include <spm_svc.h>
#include <string.h>

__dead2 void secure_services_loop(void)
{
	int32_t event_status_code;
	svc_args svc_values = { 0 };

	/*
	 * The first time this loop is executed corresponds to when Cactus has
	 * finished initialising its run time environment and is ready to handle
	 * secure service requests.
	 */
	NOTICE("Cactus: Signal end of init to SPM\n");
	event_status_code = SPRT_SUCCESS;

	while (1) {
		svc_values.arg0 = SPRT_REQUEST_COMPLETE_BLOCKING_AARCH64;
		svc_values.arg1 = event_status_code;
		int32_t event_id = sp_svc(&svc_values);

		switch (event_id) {

		default:
			NOTICE("Unhandled Service ID 0x%x\n", event_id);
			event_status_code = SPRT_NOT_SUPPORTED;
			break;
		}
	}
}
