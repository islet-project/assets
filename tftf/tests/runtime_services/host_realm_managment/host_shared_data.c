/*
 * Copyright (c) 2022, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <string.h>

#include <host_realm_mem_layout.h>
#include <host_shared_data.h>

static host_shared_data_t *host_shared_data =
		((host_shared_data_t *)(NS_REALM_SHARED_MEM_BASE));
static host_shared_data_t *guest_shared_data;
/*
 * Return shared buffer pointer mapped as host_shared_data_t structure
 */
host_shared_data_t *host_get_shared_structure(void)
{
	return host_shared_data;
}

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
 * Set data to be shared from Host to realm
 */
void realm_shared_data_set_host_val(uint8_t index, u_register_t val)
{
	host_shared_data->host_param_val[(index >= MAX_DATA_SIZE) ?
		(MAX_DATA_SIZE - 1) : index] = val;
}

/*
 * Set data to be shared from realm to Host
 */
void realm_shared_data_set_realm_val(uint8_t index, u_register_t val)
{
	host_shared_data->realm_out_val[(index >= MAX_DATA_SIZE) ?
		(MAX_DATA_SIZE - 1) : index] = val;
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
 * Return Realm's data at index
 */
u_register_t realm_shared_data_get_realm_val(uint8_t index)
{
	return host_shared_data->realm_out_val[(index >= MAX_DATA_SIZE) ?
		(MAX_DATA_SIZE - 1) : index];
}

/*
 * Clear shared realm data
 */
void realm_shared_data_clear_realm_val(void)
{
	(void)memset((char *)host_shared_data->realm_out_val, 0,
	MAX_DATA_SIZE * sizeof(host_shared_data->realm_out_val[0]));
}

/*
 * Clear shared Host data
 */
void realm_shared_data_clear_host_val(void)
{
	(void)memset((char *)host_shared_data->host_param_val, 0,
	MAX_DATA_SIZE * sizeof(host_shared_data->host_param_val[0]));
}

/*
 * Get command sent from Host to realm
 */
uint8_t realm_shared_data_get_realm_cmd(void)
{
	return guest_shared_data->realm_cmd;
}

/*
 * Set command to be send from Host to realm
 */
void realm_shared_data_set_realm_cmd(uint8_t cmd)
{
	host_shared_data->realm_cmd = cmd;
}
