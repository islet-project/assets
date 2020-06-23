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

#ifndef __ASSEMBLY__

#include <stdint.h>

smc_ret_values ffa_msg_send_direct_req(uint32_t source_id, uint32_t dest_id, uint32_t message);
smc_ret_values ffa_msg_send_direct_req64(uint32_t source_id, uint32_t dest_id, uint64_t message);
smc_ret_values ffa_run(uint32_t dest_id, uint32_t vcpu_id);
smc_ret_values ffa_version(uint32_t input_version);

#endif /* __ASSEMBLY__ */

#endif /* FFA_HELPERS_H */
