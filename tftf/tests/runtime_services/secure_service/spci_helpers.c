/*
 * Copyright (c) 2018-2020, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <debug.h>
#include <smccc.h>
#include <spci_helpers.h>
#include <spci_svc.h>

/*-----------------------------------------------------------------------------
 * SPCI_RUN
 *
 * Parameters
 *     uint32 Function ID (w0): 0x8400006D
 *     uint32 Target information (w1): Information to identify target SP/VM
 *         -Bits[31:16]: ID of SP/VM.
 *         -Bits[15:0]: ID of vCPU of SP/VM to run.
 *     Other Parameter registers w2-w7/x2-x7: Reserved (MBZ)
 *
 * On failure, returns SPCI_ERROR in w0 and error code in w2:
 *     -INVALID_PARAMETERS: Unrecognized endpoint or vCPU ID
 *     -NOT_SUPPORTED: This function is not implemented at this SPCI instance
 *     -DENIED: Callee is not in a state to handle this request
 *     -BUSY: vCPU is busy and caller must retry later
 *     -ABORTED: vCPU or VM ran into an unexpected error and has aborted
 */
smc_ret_values spci_run(uint32_t dest_id, uint32_t vcpu_id)
{
	smc_args args = {
		SPCI_MSG_RUN,
		(dest_id << 16) | vcpu_id,
		0, 0, 0, 0, 0, 0
	};

	return tftf_smc(&args);
}

/*-----------------------------------------------------------------------------
 * SPCI_MSG_SEND_DIRECT_REQ
 *
 * Parameters
 *     uint32 Function ID (w0): 0x8400006F / 0xC400006F
 *     uint32 Source/Destination IDs (w1): Source and destination endpoint IDs
 *         -Bit[31:16]: Source endpoint ID
 *         -Bit[15:0]: Destination endpoint ID
 *     uint32/uint64 (w2/x2) - RFU MBZ
 *     w3-w7 - Implementation defined
 *
 * On failure, returns SPCI_ERROR in w0 and error code in w2:
 *     -INVALID_PARAMETERS: Invalid endpoint ID or non-zero reserved register
 *     -DENIED: Callee is not in a state to handle this request
 *     -NOT_SUPPORTED: This function is not implemented at this SPCI instance
 *     -BUSY: Message target is busy
 *     -ABORTED: Message target ran into an unexpected error and has aborted
 */
static smc_ret_values __spci_msg_send_direct_req32_5(uint32_t source_id,
						     uint32_t dest_id,
						     uint32_t arg0,
						     uint32_t arg1,
						     uint32_t arg2,
						     uint32_t arg3,
						     uint32_t arg4)
{
	smc_args args = {
		SPCI_MSG_SEND_DIRECT_REQ_SMC32,
		(source_id << 16) | dest_id,
		0,
		arg0, arg1, arg2, arg3, arg4
	};

	return tftf_smc(&args);
}

/* Direct message send helper accepting a single 32b message argument */
smc_ret_values spci_msg_send_direct_req(uint32_t source_id, uint32_t dest_id,
					uint32_t message)
{
	return __spci_msg_send_direct_req32_5(source_id, dest_id,
					      message, 0, 0, 0, 0);
}

static smc_ret_values __spci_msg_send_direct_req64_5(uint32_t source_id,
						     uint32_t dest_id,
						     uint64_t arg0,
						     uint64_t arg1,
						     uint64_t arg2,
						     uint64_t arg3,
						     uint64_t arg4)
{
	smc_args args = {
		SPCI_MSG_SEND_DIRECT_REQ_SMC64,
		(source_id << 16) | dest_id,
		0,
		arg0, arg1, arg2, arg3, arg4
	};

	return tftf_smc(&args);
}

/* Direct message send helper accepting a single 64b message argument */
smc_ret_values spci_msg_send_direct_req64(uint32_t source_id, uint32_t dest_id,
					uint64_t message)
{
	return __spci_msg_send_direct_req64_5(source_id, dest_id,
					      message, 0, 0, 0, 0);
}
