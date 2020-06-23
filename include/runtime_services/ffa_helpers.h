/*
 * Copyright (c) 2018-2020, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef FFA_HELPERS_H
#define FFA_HELPERS_H

#include <ffa_svc.h>
#include <tftf_lib.h>
#include <utils_def.h>

/* This error code must be different to the ones used by FFA */
#define FFA_TFTF_ERROR		-42

/* Hypervisor ID at physical FFA instance */
#define HYP_ID          (0)

/* By convention, SP IDs (as opposed to VM IDs) have bit 15 set */
#define SP_ID(x)        ((x) | (1 << 15))

typedef unsigned short ffa_vm_id_t;
typedef unsigned short ffa_vm_count_t;
typedef unsigned short ffa_vcpu_count_t;

#ifndef __ASSEMBLY__

#include <stdint.h>

struct mailbox_buffers {
	const void *recv;
	void *send;
};

struct ffa_partition_info {
	/** The ID of the VM the information is about */
	ffa_vm_id_t id;
	/** The number of execution contexts implemented by the partition */
	uint16_t exec_context;
	/** The Partition's properties, e.g. supported messaging methods */
	uint32_t properties;
};

/*
 * TODO: In the future this file should be placed in a common folder, and not
 * under tftf. The functions in this file are also used by SPs for SPM tests.
 */
bool check_spmc_execution_level(void);
smc_ret_values ffa_msg_send_direct_req(uint32_t source_id, uint32_t dest_id, uint32_t message);
smc_ret_values ffa_msg_send_direct_req64(uint32_t source_id, uint32_t dest_id, uint64_t message);
smc_ret_values ffa_run(uint32_t dest_id, uint32_t vcpu_id);
smc_ret_values ffa_version(uint32_t input_version);
smc_ret_values ffa_id_get(void);
smc_ret_values ffa_msg_wait(void);
smc_ret_values ffa_msg_send_direct_resp(ffa_vm_id_t source_id,
					ffa_vm_id_t dest_id, uint32_t message);
smc_ret_values ffa_error(int32_t error_code);
smc_ret_values ffa_features(uint32_t feature);
smc_ret_values ffa_partition_info_get(const uint32_t uuid[4]);
smc_ret_values ffa_rx_release(void);

#endif /* __ASSEMBLY__ */

#endif /* FFA_HELPERS_H */
