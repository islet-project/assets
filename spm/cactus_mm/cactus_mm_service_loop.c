/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>
#include <debug.h>
#include <mm_svc.h>
#include <secure_partition.h>
#include <sp_helpers.h>
#include <spm_svc.h>
#include <string.h>


/*
 * Handle a fast secure service request, i.e. one made through an MM_COMMUNICATE
 * call.
 *
 * cc
 *   Calling convention. If MM_COMMUNICATE has been invoked using the SMC32
 *   calling convention, this argument must be 32, else 64.
 *
 * sps
 *   Communication buffer attached to the secure partition service request.
 */
static int32_t cactus_handle_fast_request(int cc,
					  secure_partition_request_info_t *sps)
{
	assert(cc == 32 || cc == 64);

	/* No SMC32 is supported at the moment. Just ignore them. */
	if (cc == 32) {
		INFO("Ignoring MM_COMMUNICATE_AARCH32 call\n");
		return SPM_SUCCESS;
	}

	/* See secure_partition.h for possible ID values. */
	switch (sps->id) {
	case SPS_TIMER_SLEEP: {
		if (sps->data_size != 1) {
			ERROR("Invalid payload size for SPM_SPS_TIMER_SLEEP request (%llu)\n",
			      sps->data_size);
			return SPM_INVALID_PARAMETER;
		}
		int duration_sec = sps->data[0];
		sp_sleep(duration_sec);

		/*
		 * Write back to the communication buffer to acknowledge the
		 * request has been successfully handled.
		 */
		uint32_t response = CACTUS_FAST_REQUEST_SUCCESS;
		memcpy(sps->data, &response, sizeof(response));
		return SPM_SUCCESS;
	}

	case SPS_CHECK_ALIVE:
		return SPM_SUCCESS;

	default:
		INFO("Unsupported MM_COMMUNICATE_AARCH64 call with service ID 0x%x, ignoring it\n",
		     sps->id);
		return SPM_INVALID_PARAMETER;
	}
}

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
	event_status_code = SPM_SUCCESS;

	while (1) {
		svc_values.fid = SP_EVENT_COMPLETE_AARCH64;
		svc_values.arg1 = event_status_code;
		int32_t event_id = sp_svc(&svc_values);

		switch (event_id) {
		case MM_COMMUNICATE_AARCH64:
		  {
			uint64_t ctx_addr = svc_values.arg1;
			uint32_t ctx_size = svc_values.arg2;
			uint64_t cookie = svc_values.arg3;

			NOTICE("Cactus: Received MM_COMMUNICATE_AARCH64 call\n");
			NOTICE("Cactus:   Context address: 0x%llx\n", ctx_addr);
			NOTICE("Cactus:   Context size   : %u\n", ctx_size);
			NOTICE("Cactus:   Cookie         : 0x%llx\n", cookie);

			if (ctx_addr == 0) {
				ERROR("Context address is invalid\n");
				event_status_code = SPM_INVALID_PARAMETER;
				continue;
			}

			secure_partition_request_info_t *sps = (void *)(uintptr_t) ctx_addr;
			NOTICE("Received fast secure service request with ID #%u\n",
			       sps->id);
			event_status_code = cactus_handle_fast_request(64, sps);
			break;
		  }

		case MM_COMMUNICATE_AARCH32:
		  {
			uint32_t ctx_addr = svc_values.arg1;
			uint32_t ctx_size = svc_values.arg2;
			uint32_t cookie = svc_values.arg3;

			NOTICE("Cactus: Received MM_COMMUNICATE_AARCH32 call\n");
			NOTICE("Cactus:   Context address: 0x%x\n", ctx_addr);
			NOTICE("Cactus:   Context size   : %u\n", ctx_size);
			NOTICE("Cactus:   Cookie         : 0x%x\n", cookie);

			if (ctx_addr == 0) {
				ERROR("Context address is invalid\n");
				event_status_code = SPM_INVALID_PARAMETER;
				continue;
			}

			secure_partition_request_info_t *sps = (void *)(uintptr_t) ctx_addr;
			NOTICE("Received fast secure service request with ID #%u\n",
			       sps->id);
			event_status_code = cactus_handle_fast_request(32, sps);
			break;
		  }

		default:
			NOTICE("Unhandled Service ID 0x%x\n", event_id);
			event_status_code = SPM_NOT_SUPPORTED;
			break;
		}
	}
}
