/*
 * Copyright (c) 2022, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <string.h>
#include <host_shared_data.h>

/**
 *   @brief    - Returns the base address of the shared region
 *   @param    - Void
 *   @return   - Base address of the shared region
 **/

static host_shared_data_t *guest_shared_data;

/*
 * Set guest mapped shared buffer pointer
 */
void realm_set_shared_structure(host_shared_data_t *ptr)
{
	guest_shared_data = ptr;
}

/*
 * Get guest mapped shared buffer pointer
 */
host_shared_data_t *realm_get_shared_structure(void)
{
	return guest_shared_data;
}

/*
 * Return Host's data at index
 */
u_register_t realm_shared_data_get_host_val(uint8_t index)
{
	return guest_shared_data->host_param_val[(index >= MAX_DATA_SIZE) ?
		(MAX_DATA_SIZE - 1) : index];
}

/*
 * Get command sent from Host to realm
 */
uint8_t realm_shared_data_get_realm_cmd(void)
{
	return guest_shared_data->realm_cmd;
}
