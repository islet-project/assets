/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef SPCI_HELPERS_H
#define SPCI_HELPERS_H

#include <utils_def.h>

/* This error code must be different to the ones used by SPCI */
#define SPCI_TFTF_ERROR		-42

/* Client ID used for SPCI calls */
#define TFTF_SPCI_CLIENT_ID	U(0x00007F7F)

#ifndef __ASSEMBLY__

#include <stdint.h>
#include <types.h>

int spci_service_handle_open(uint16_t client_id, uint16_t *handle,
			     uint32_t uuid1, uint32_t uuid2,
			     uint32_t uuid3, uint32_t uuid4);
int spci_service_handle_close(uint16_t client_id, uint16_t handle);

#endif /* __ASSEMBLY__ */

#endif /* SPCI_HELPERS_H */
