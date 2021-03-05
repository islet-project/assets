/*
 * Copyright (c) 2018-2021, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <smccc.h>

#include <ffa_endpoints.h>
#include <ffa_helpers.h>
#include <ffa_svc.h>

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
smc_ret_values ffa_msg_send_direct_req64(ffa_id_t source_id,
					 ffa_id_t dest_id, uint64_t arg0,
					 uint64_t arg1, uint64_t arg2,
					 uint64_t arg3, uint64_t arg4)
{
	smc_args args = {
		.fid = FFA_MSG_SEND_DIRECT_REQ_SMC64,
		.arg1 = ((uint32_t)(source_id << 16)) | (dest_id),
		.arg2 = 0,
		.arg3 = arg0,
		.arg4 = arg1,
		.arg5 = arg2,
		.arg6 = arg3,
		.arg7 = arg4,
	};

	return tftf_smc(&args);
}

smc_ret_values ffa_msg_send_direct_req32(ffa_id_t source_id,
					 ffa_id_t dest_id, uint32_t arg0,
					 uint32_t arg1, uint32_t arg2,
					 uint32_t arg3, uint32_t arg4)
{
	smc_args args = {
		.fid = FFA_MSG_SEND_DIRECT_REQ_SMC32,
		.arg1 = ((uint32_t)(source_id << 16)) | (dest_id),
		.arg2 = 0,
		.arg3 = arg0,
		.arg4 = arg1,
		.arg5 = arg2,
		.arg6 = arg3,
		.arg7 = arg4,
	};

	return tftf_smc(&args);
}

smc_ret_values ffa_msg_send_direct_resp64(ffa_id_t source_id,
					  ffa_id_t dest_id, uint64_t arg0,
					  uint64_t arg1, uint64_t arg2,
					  uint64_t arg3, uint64_t arg4)
{
	smc_args args = {
		.fid = FFA_MSG_SEND_DIRECT_RESP_SMC64,
		.arg1 = ((uint32_t)(source_id << 16)) | (dest_id),
		.arg2 = 0,
		.arg3 = arg0,
		.arg4 = arg1,
		.arg5 = arg2,
		.arg6 = arg3,
		.arg7 = arg4,
	};

	return tftf_smc(&args);
}

smc_ret_values ffa_msg_send_direct_resp32(ffa_id_t source_id,
					  ffa_id_t dest_id, uint32_t arg0,
					  uint32_t arg1, uint32_t arg2,
					  uint32_t arg3, uint32_t arg4)
{
	smc_args args = {
		.fid = FFA_MSG_SEND_DIRECT_RESP_SMC32,
		.arg1 = ((uint32_t)(source_id << 16)) | (dest_id),
		.arg2 = 0,
		.arg3 = arg0,
		.arg4 = arg1,
		.arg5 = arg2,
		.arg6 = arg3,
		.arg7 = arg4,
	};

	return tftf_smc(&args);
}


/**
 * Initialises the header of the given `ffa_memory_region`, not including the
 * composite memory region offset.
 */
static void ffa_memory_region_init_header(
	struct ffa_memory_region *memory_region, ffa_id_t sender,
	ffa_memory_attributes_t attributes, ffa_memory_region_flags_t flags,
	ffa_memory_handle_t handle, uint32_t tag, ffa_id_t receiver,
	ffa_memory_access_permissions_t permissions)
{
	memory_region->sender = sender;
	memory_region->attributes = attributes;
	memory_region->reserved_0 = 0;
	memory_region->flags = flags;
	memory_region->handle = handle;
	memory_region->tag = tag;
	memory_region->reserved_1 = 0;
	memory_region->receiver_count = 1;
	memory_region->receivers[0].receiver_permissions.receiver = receiver;
	memory_region->receivers[0].receiver_permissions.permissions =
		permissions;
	memory_region->receivers[0].receiver_permissions.flags = 0;
	memory_region->receivers[0].reserved_0 = 0;
}

/**
 * Initialises the given `ffa_memory_region` and copies as many as possible of
 * the given constituents to it.
 *
 * Returns the number of constituents remaining which wouldn't fit, and (via
 * return parameters) the size in bytes of the first fragment of data copied to
 * `memory_region` (attributes, constituents and memory region header size), and
 * the total size of the memory sharing message including all constituents.
 */
uint32_t ffa_memory_region_init(
	struct ffa_memory_region *memory_region, size_t memory_region_max_size,
	ffa_id_t sender, ffa_id_t receiver,
	const struct ffa_memory_region_constituent constituents[],
	uint32_t constituent_count, uint32_t tag,
	ffa_memory_region_flags_t flags, enum ffa_data_access data_access,
	enum ffa_instruction_access instruction_access,
	enum ffa_memory_type type, enum ffa_memory_cacheability cacheability,
	enum ffa_memory_shareability shareability, uint32_t *total_length,
	uint32_t *fragment_length)
{
	ffa_memory_access_permissions_t permissions = 0;
	ffa_memory_attributes_t attributes = 0;
	struct ffa_composite_memory_region *composite_memory_region;
	uint32_t fragment_max_constituents;
	uint32_t count_to_copy;
	uint32_t i;
	uint32_t constituents_offset;

	/* Set memory region's permissions. */
	ffa_set_data_access_attr(&permissions, data_access);
	ffa_set_instruction_access_attr(&permissions, instruction_access);

	/* Set memory region's page attributes. */
	ffa_set_memory_type_attr(&attributes, type);
	ffa_set_memory_cacheability_attr(&attributes, cacheability);
	ffa_set_memory_shareability_attr(&attributes, shareability);

	ffa_memory_region_init_header(memory_region, sender, attributes, flags,
				      0, tag, receiver, permissions);
	/*
	 * Note that `sizeof(struct_ffa_memory_region)` and `sizeof(struct
	 * ffa_memory_access)` must both be multiples of 16 (as verified by the
	 * asserts in `ffa_memory.c`, so it is guaranteed that the offset we
	 * calculate here is aligned to a 64-bit boundary and so 64-bit values
	 * can be copied without alignment faults.
	 */
	memory_region->receivers[0].composite_memory_region_offset =
		sizeof(struct ffa_memory_region) +
		memory_region->receiver_count *
			sizeof(struct ffa_memory_access);

	composite_memory_region =
		ffa_memory_region_get_composite(memory_region, 0);
	composite_memory_region->page_count = 0;
	composite_memory_region->constituent_count = constituent_count;
	composite_memory_region->reserved_0 = 0;

	constituents_offset =
		memory_region->receivers[0].composite_memory_region_offset +
		sizeof(struct ffa_composite_memory_region);
	fragment_max_constituents =
		(memory_region_max_size - constituents_offset) /
		sizeof(struct ffa_memory_region_constituent);

	count_to_copy = constituent_count;
	if (count_to_copy > fragment_max_constituents) {
		count_to_copy = fragment_max_constituents;
	}

	for (i = 0; i < constituent_count; ++i) {
		if (i < count_to_copy) {
			composite_memory_region->constituents[i] =
				constituents[i];
		}
		composite_memory_region->page_count +=
			constituents[i].page_count;
	}

	if (total_length != NULL) {
		*total_length =
			constituents_offset +
			composite_memory_region->constituent_count *
				sizeof(struct ffa_memory_region_constituent);
	}
	if (fragment_length != NULL) {
		*fragment_length =
			constituents_offset +
			count_to_copy *
				sizeof(struct ffa_memory_region_constituent);
	}

	return composite_memory_region->constituent_count - count_to_copy;
}

/**
 * Initialises the given `ffa_memory_region` to be used for an
 * `FFA_MEM_RETRIEVE_REQ` by the receiver of a memory transaction.
 *
 * Returns the size of the message written.
 */
uint32_t ffa_memory_retrieve_request_init(
	struct ffa_memory_region *memory_region, ffa_memory_handle_t handle,
	ffa_id_t sender, ffa_id_t receiver, uint32_t tag,
	ffa_memory_region_flags_t flags, enum ffa_data_access data_access,
	enum ffa_instruction_access instruction_access,
	enum ffa_memory_type type, enum ffa_memory_cacheability cacheability,
	enum ffa_memory_shareability shareability)
{
	ffa_memory_access_permissions_t permissions = 0;
	ffa_memory_attributes_t attributes = 0;

	/* Set memory region's permissions. */
	ffa_set_data_access_attr(&permissions, data_access);
	ffa_set_instruction_access_attr(&permissions, instruction_access);

	/* Set memory region's page attributes. */
	ffa_set_memory_type_attr(&attributes, type);
	ffa_set_memory_cacheability_attr(&attributes, cacheability);
	ffa_set_memory_shareability_attr(&attributes, shareability);

	ffa_memory_region_init_header(memory_region, sender, attributes, flags,
					handle, tag, receiver, permissions);
	/*
	 * Offset 0 in this case means that the hypervisor should allocate the
	 * address ranges. This is the only configuration supported by Hafnium,
	 * as it enforces 1:1 mappings in the stage 2 page tables.
	 */
	memory_region->receivers[0].composite_memory_region_offset = 0;
	memory_region->receivers[0].reserved_0 = 0;

	return sizeof(struct ffa_memory_region) +
	       memory_region->receiver_count * sizeof(struct ffa_memory_access);
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

smc_ret_values ffa_spm_id_get(void)
{
	smc_args args = {
		.fid = FFA_SPM_ID_GET
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
smc_ret_values ffa_partition_info_get(const struct ffa_uuid uuid)
{
	smc_args args = {
		.fid = FFA_PARTITION_INFO_GET,
		.arg1 = uuid.uuid[0],
		.arg2 = uuid.uuid[1],
		.arg3 = uuid.uuid[2],
		.arg4 = uuid.uuid[3]
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

/* Map the RXTX buffer */
smc_ret_values ffa_rxtx_map(uintptr_t send, uintptr_t recv, uint32_t pages)
{
	smc_args args = {
		.fid = FFA_RXTX_MAP_SMC64,
		.arg1 = send,
		.arg2 = recv,
		.arg3 = pages
	};

	return tftf_smc(&args);
}

/* Donate memory to another partition */
smc_ret_values ffa_mem_donate(uint32_t descriptor_length,
				uint32_t fragment_length)
{
	smc_args args = {
		.fid = FFA_MEM_DONATE_SMC32,
		.arg1 = descriptor_length,
		.arg2 = fragment_length,
		.arg3 = FFA_PARAM_MBZ,
		.arg4 = FFA_PARAM_MBZ
	};

	return tftf_smc(&args);
}

/* Lend memory to another partition */
smc_ret_values ffa_mem_lend(uint32_t descriptor_length,
				uint32_t fragment_length)
{
	smc_args args = {
		.fid = FFA_MEM_LEND_SMC32,
		.arg1 = descriptor_length,
		.arg2 = fragment_length,
		.arg3 = FFA_PARAM_MBZ,
		.arg4 = FFA_PARAM_MBZ
	};

	return tftf_smc(&args);
}

/* Share memory with another partition */
smc_ret_values ffa_mem_share(uint32_t descriptor_length,
				uint32_t fragment_length)
{
	smc_args args = {
		.fid = FFA_MEM_SHARE_SMC32,
		.arg1 = descriptor_length,
		.arg2 = fragment_length,
		.arg3 = FFA_PARAM_MBZ,
		.arg4 = FFA_PARAM_MBZ
	};

	return tftf_smc(&args);
}

/* Retrieve memory shared by another partition */
smc_ret_values ffa_mem_retrieve_req(uint32_t descriptor_length,
					uint32_t fragment_length)
{
	smc_args args = {
		.fid = FFA_MEM_RETRIEVE_REQ_SMC32,
		.arg1 = descriptor_length,
		.arg2 = fragment_length,
		.arg3 = FFA_PARAM_MBZ,
		.arg4 = FFA_PARAM_MBZ,
		.arg5 = FFA_PARAM_MBZ,
		.arg6 = FFA_PARAM_MBZ,
		.arg7 = FFA_PARAM_MBZ
	};

	return tftf_smc(&args);
}

/* Relinquish access to memory region */
smc_ret_values ffa_mem_relinquish(void)
{
	smc_args args = {
		.fid = FFA_MEM_RELINQUISH,
	};

	return tftf_smc(&args);
}

/* Reclaim exclusive access to owned memory region */
smc_ret_values ffa_mem_reclaim(uint64_t handle, uint32_t flags)
{
	smc_args args = {
		.fid = FFA_MEM_RECLAIM,
		.arg1 = (uint32_t) handle,
		.arg2 = (uint32_t) (handle >> 32),
		.arg3 = flags
	};

	return tftf_smc(&args);
}
