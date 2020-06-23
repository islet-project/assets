/*
 * Copyright (c) 2018-2020, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <debug.h>
#include <smccc.h>
#include <ffa_helpers.h>
#include <ffa_svc.h>

#define OPTEE_FFA_GET_API_VERSION	(0)
#define OPTEE_FFA_GET_OS_VERSION	(1)
#define OPTEE_FFA_GET_OS_VERSION_MAJOR	(3)
#define OPTEE_FFA_GET_OS_VERSION_MINOR	(8)

/*-----------------------------------------------------------------------------
 * FFA_RUN
 *
 * Parameters
 *     uint32 Function ID (w0): 0x8400006D
 *     uint32 Target information (w1): Information to identify target SP/VM
 *         -Bits[31:16]: ID of SP/VM.
 *         -Bits[15:0]: ID of vCPU of SP/VM to run.
 *     Other Parameter registers w2-w7/x2-x7: Reserved (MBZ)
 *
 * On failure, returns FFA_ERROR in w0 and error code in w2:
 *     -INVALID_PARAMETERS: Unrecognized endpoint or vCPU ID
 *     -NOT_SUPPORTED: This function is not implemented at this FFA instance
 *     -DENIED: Callee is not in a state to handle this request
 *     -BUSY: vCPU is busy and caller must retry later
 *     -ABORTED: vCPU or VM ran into an unexpected error and has aborted
 */
smc_ret_values ffa_run(uint32_t dest_id, uint32_t vcpu_id)
{
	smc_args args = {
		FFA_MSG_RUN,
		(dest_id << 16) | vcpu_id,
		0, 0, 0, 0, 0, 0
	};

	return tftf_smc(&args);
}

/*-----------------------------------------------------------------------------
 * FFA_MSG_SEND_DIRECT_REQ
 *
 * Parameters
 *     uint32 Function ID (w0): 0x8400006F / 0xC400006F
 *     uint32 Source/Destination IDs (w1): Source and destination endpoint IDs
 *         -Bit[31:16]: Source endpoint ID
 *         -Bit[15:0]: Destination endpoint ID
 *     uint32/uint64 (w2/x2) - RFU MBZ
 *     w3-w7 - Implementation defined
 *
 * On failure, returns FFA_ERROR in w0 and error code in w2:
 *     -INVALID_PARAMETERS: Invalid endpoint ID or non-zero reserved register
 *     -DENIED: Callee is not in a state to handle this request
 *     -NOT_SUPPORTED: This function is not implemented at this FFA instance
 *     -BUSY: Message target is busy
 *     -ABORTED: Message target ran into an unexpected error and has aborted
 */
static smc_ret_values __ffa_msg_send_direct_req32_5(uint32_t source_id,
						     uint32_t dest_id,
						     uint32_t arg0,
						     uint32_t arg1,
						     uint32_t arg2,
						     uint32_t arg3,
						     uint32_t arg4)
{
	smc_args args = {
		FFA_MSG_SEND_DIRECT_REQ_SMC32,
		(source_id << 16) | dest_id,
		0,
		arg0, arg1, arg2, arg3, arg4
	};

	return tftf_smc(&args);
}

/* Direct message send helper accepting a single 32b message argument */
smc_ret_values ffa_msg_send_direct_req(uint32_t source_id, uint32_t dest_id,
					uint32_t message)
{
	return __ffa_msg_send_direct_req32_5(source_id, dest_id,
					      message, 0, 0, 0, 0);
}

static smc_ret_values __ffa_msg_send_direct_req64_5(uint32_t source_id,
						     uint32_t dest_id,
						     uint64_t arg0,
						     uint64_t arg1,
						     uint64_t arg2,
						     uint64_t arg3,
						     uint64_t arg4)
{
	smc_args args = {
		FFA_MSG_SEND_DIRECT_REQ_SMC64,
		(source_id << 16) | dest_id,
		0,
		arg0, arg1, arg2, arg3, arg4
	};

	return tftf_smc(&args);
}

/* Direct message send helper accepting a single 64b message argument */
smc_ret_values ffa_msg_send_direct_req64(uint32_t source_id, uint32_t dest_id,
					uint64_t message)
{
	return __ffa_msg_send_direct_req64_5(source_id, dest_id,
					      message, 0, 0, 0, 0);
}

/*
 * check_spmc_execution_level
 *
 * Attempt sending impdef protocol messages to OP-TEE through direct messaging.
 * Criteria for detecting OP-TEE presence is that responses match defined
 * version values. In the case of SPMC running at S-EL2 (and Cactus instances
 * running at S-EL1) the response will not match the pre-defined version IDs.
 *
 * Returns true if SPMC is probed as being OP-TEE at S-EL1.
 *
 */
bool check_spmc_execution_level(void)
{
	unsigned int is_optee_spmc_criteria = 0U;
	smc_ret_values ret_values;

	/*
	 * Send a first OP-TEE-defined protocol message through
	 * FFA direct message.
	 *
	 */
	ret_values = ffa_msg_send_direct_req(HYP_ID, SP_ID(1),
					      OPTEE_FFA_GET_API_VERSION);
	if ((ret_values.ret3 == FFA_VERSION_MAJOR) &&
	    (ret_values.ret4 == FFA_VERSION_MINOR)) {
		is_optee_spmc_criteria++;
	}

	/*
	 * Send a second OP-TEE-defined protocol message through
	 * FFA direct message.
	 *
	 */
	ret_values = ffa_msg_send_direct_req(HYP_ID, SP_ID(1),
					      OPTEE_FFA_GET_OS_VERSION);
	if ((ret_values.ret3 == OPTEE_FFA_GET_OS_VERSION_MAJOR) &&
	    (ret_values.ret4 == OPTEE_FFA_GET_OS_VERSION_MINOR)) {
		is_optee_spmc_criteria++;
	}

	return (is_optee_spmc_criteria == 2U);
}

/*
 * FFA Version ABI helper.
 * Version fields:
 *	-Bits[30:16]: Major version.
 *	-Bits[15:0]: Minor version.
 */
smc_ret_values ffa_version(uint32_t input_version)
{
	smc_args args = {
		.fid = FFA_VERSION,
		.arg1 = input_version
	};

	return tftf_smc(&args);
}

smc_ret_values ffa_id_get(void)
{
	smc_args args = {
		.fid = FFA_ID_GET
	};

	return tftf_smc(&args);
}

smc_ret_values ffa_msg_wait(void)
{
	smc_args args = {
		.fid = FFA_MSG_WAIT
	};

	return tftf_smc(&args);
}

smc_ret_values ffa_msg_send_direct_resp(ffa_vm_id_t source_id,
						ffa_vm_id_t dest_id,
						uint32_t message)
{
	smc_args args = {
		.fid = FFA_MSG_SEND_DIRECT_RESP_SMC32,
		.arg1 = ((uint32_t)source_id << 16) | dest_id,
		.arg3 = message
	};

	return tftf_smc(&args);
}

smc_ret_values ffa_error(int32_t error_code)
{
	smc_args args = {
		.fid = FFA_ERROR,
		.arg1 = 0,
		.arg2 = error_code
	};

	return tftf_smc(&args);
}

/* Query the higher EL if the requested FF-A feature is implemented. */
smc_ret_values ffa_features(uint32_t feature)
{
	smc_args args = {
		.fid = FFA_FEATURES,
		.arg1 = feature
	};

	return tftf_smc(&args);
}

/* Get information about VMs or SPs based on UUID */
smc_ret_values ffa_partition_info_get(const uint32_t uuid[4])
{
	smc_args args = {
		.fid = FFA_PARTITION_INFO_GET,
		.arg1 = uuid[0],
		.arg2 = uuid[1],
		.arg3 = uuid[2],
		.arg4 = uuid[3]
	};

	return tftf_smc(&args);
}

/* Query SPMD that the rx buffer of the partition can be released */
smc_ret_values ffa_rx_release(void)
{
	smc_args args = {
		.fid = FFA_RX_RELEASE
	};

	return tftf_smc(&args);
}
