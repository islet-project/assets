/*
 * Copyright (c) 2018-2020, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef SPCI_HELPERS_H
#define SPCI_HELPERS_H

#include <tftf_lib.h>
#include <utils_def.h>

/* This error code must be different to the ones used by SPCI */
#define SPCI_TFTF_ERROR		-42

#ifndef __ASSEMBLY__

#include <stdint.h>

smc_ret_values spci_msg_send_direct_req(uint32_t source_id, uint32_t dest_id, uint32_t message);
smc_ret_values spci_msg_send_direct_req64(uint32_t source_id, uint32_t dest_id, uint64_t message);
smc_ret_values spci_run(uint32_t dest_id, uint32_t vcpu_id);

#endif /* __ASSEMBLY__ */

#endif /* SPCI_HELPERS_H */
