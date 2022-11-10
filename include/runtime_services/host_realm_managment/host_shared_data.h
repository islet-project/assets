/*
 * Copyright (c) 2022, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef HOST_SHARED_DATA_H
#define HOST_SHARED_DATA_H

#include <stdint.h>
#include <spinlock.h>

#define MAX_BUF_SIZE		10240U
#define MAX_DATA_SIZE		5U

/*
 * This structure maps the shared memory to be used between the Host and Realm
 * payload
 */
typedef struct host_shared_data {
	/* Buffer used from Realm for logging*/
	uint8_t log_buffer[MAX_BUF_SIZE];

	/* Command set from Host and used by Realm*/
	uint8_t realm_cmd;

	/* array of params passed from Host to Realm*/
	u_register_t host_param_val[MAX_DATA_SIZE];

	/* array of output results passed from Realm to Host*/
	u_register_t realm_out_val[MAX_DATA_SIZE];

	/* Lock to avoid concurrent accesses to log_buffer */
	spinlock_t printf_lock;
} host_shared_data_t;

/*
 * Different commands that the Host can requests the Realm to perform
 */
enum realm_cmd {
	REALM_SLEEP_CMD = 1U,
	REALM_GET_RSI_VERSION
};

/*
 * Index values for each parameter in the host_param_val array.
 */
enum host_param_index {
	HOST_CMD_INDEX = 0U,
	HOST_SLEEP_INDEX
};
/*
 * Return shared buffer pointer mapped as host_shared_data_t structure
 */
host_shared_data_t *host_get_shared_structure(void);
/*
 * Set data to be shared from Host to realm
 */
void realm_shared_data_set_host_val(uint8_t index, u_register_t val);

/*
 * Set guest mapped shared buffer pointer
 */
void realm_set_shared_structure(host_shared_data_t *ptr);

/*
 * Get guest mapped shared buffer pointer
 */
host_shared_data_t *realm_get_shared_structure(void);

/*
 * Set data to be shared from Realm to Host
 */
void realm_shared_data_set_realm_val(uint8_t index, u_register_t val);

/*
 * Return Host's data at index
 */
u_register_t realm_shared_data_get_host_val(uint8_t index);

/*
 * Return Realm's data at index
 */
u_register_t realm_shared_data_get_realm_val(uint8_t index);

/*
 * Clear shared realm data
 */
void realm_shared_data_clear_realm_val(void);

/*
 * Clear shared Host data
 */
void realm_shared_data_clear_host_val(void);

/*
 * Get command sent from Host to realm
 */
uint8_t realm_shared_data_get_realm_cmd(void);

/*
 * Set command to be send from Host to realm
 */
void realm_shared_data_set_realm_cmd(uint8_t cmd);

#endif /* HOST_SHARED_DATA_H */
