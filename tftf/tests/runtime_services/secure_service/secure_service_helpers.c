/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <platform.h>
#include <secure_partition.h>
#include <string.h>


secure_partition_request_info_t *create_sps_request(uint32_t id,
						    const void *data,
						    uint64_t data_size)
{
	secure_partition_request_info_t *sps_request
		= (void *) ARM_SECURE_SERVICE_BUFFER_BASE;
	sps_request->id = id;
	sps_request->data_size = data_size;
	if (data_size != 0)
		memcpy(sps_request->data, data, data_size);
	return sps_request;
}
