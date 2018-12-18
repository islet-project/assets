/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <debug.h>
#include <smccc.h>
#include <spci_helpers.h>
#include <spci_svc.h>
#include <tftf_lib.h>

/* Returns a SPCI error code. On success, it also returns a 16 bit handle. */
int spci_service_handle_open(uint16_t client_id, uint16_t *handle,
			     uint32_t uuid1, uint32_t uuid2,
			     uint32_t uuid3, uint32_t uuid4)
{
	int32_t ret;

	smc_args get_handle_smc_args = {
		SPCI_SERVICE_HANDLE_OPEN,
		uuid1, uuid2, uuid3, uuid4,
		0, 0, /* Reserved - MBZ */
		client_id
	};

	smc_ret_values smc_ret = tftf_smc(&get_handle_smc_args);

	ret = smc_ret.ret0;
	if (ret != SPCI_SUCCESS)
		return ret;

	uint32_t x1 = smc_ret.ret1;

	if ((x1 & 0x0000FFFF) != 0) {
		tftf_testcase_printf("SPCI_SERVICE_HANDLE_OPEN returned x1 = 0x%08x\n", x1);
		return SPCI_TFTF_ERROR;
	}

	/* The handle is returned in the top 16 bits */
	*handle = (x1 >> 16) & 0xFFFF;

	return SPCI_SUCCESS;
}

/* Invokes SPCI_SERVICE_HANDLE_CLOSE. Returns a SPCI error code. */
int spci_service_handle_close(uint16_t client_id, uint16_t handle)
{
	smc_args close_handle_smc_args = {
		SPCI_SERVICE_HANDLE_CLOSE,
		client_id | (handle << 16)
	};

	smc_ret_values smc_ret = tftf_smc(&close_handle_smc_args);

	return (int32_t)(uint32_t)smc_ret.ret0;
}

/* Returns a SPCI error code. On success, it also returns a token. */
int spci_service_request_start(u_register_t x1, u_register_t x2,
			       u_register_t x3, u_register_t x4,
			       u_register_t x5, u_register_t x6,
			       uint16_t client_id, uint16_t handle,
			       uint32_t *token)
{
	int32_t ret;

	smc_args request_smc_args = {
		SPCI_SERVICE_REQUEST_START_AARCH64,
		x1, x2, x3, x4, x5, x6,
		client_id | (handle << 16)
	};

	smc_ret_values smc_ret = tftf_smc(&request_smc_args);

	ret = (int32_t)(uint32_t)smc_ret.ret0;
	if (ret != SPCI_SUCCESS)
		return ret;

	*token = smc_ret.ret1;

	return SPCI_SUCCESS;
}

/*
 * Returns a SPCI error code. On success, it also returns x1-x3. Any of the
 * pointers x1-x3 may be NULL in case the caller doesn't need that value.
 */
int spci_service_request_resume(uint16_t client_id, uint16_t handle,
				uint32_t token, u_register_t *x1,
				u_register_t *x2, u_register_t *x3)
{
	int32_t ret;

	smc_args request_resume_smc = {
		SPCI_SERVICE_REQUEST_RESUME_AARCH64,
		token, 0, 0, 0, 0, 0,
		client_id | (handle << 16)
	};

	smc_ret_values smc_ret = tftf_smc(&request_resume_smc);

	ret = (int32_t)(uint32_t)smc_ret.ret0;
	if (ret != SPCI_SUCCESS)
		return ret;

	if (x1 != NULL)
		*x1 = smc_ret.ret1;
	if (x2 != NULL)
		*x2 = smc_ret.ret2;
	if (x3 != NULL)
		*x3 = smc_ret.ret3;

	return SPCI_SUCCESS;
}

/*
 * Returns a SPCI error code. On success, it also returns x1-x3. Any of the
 * pointers x1-x3 may be NULL in case the caller doesn't need that value.
 */
int spci_service_get_response(uint16_t client_id, uint16_t handle,
			      uint32_t token, u_register_t *x1,
			      u_register_t *x2, u_register_t *x3)
{
	int32_t ret;

	smc_args get_response_smc = {
		SPCI_SERVICE_GET_RESPONSE_AARCH64,
		token, 0, 0, 0, 0, 0,
		client_id | (handle << 16)
	};

	smc_ret_values smc_ret = tftf_smc(&get_response_smc);

	ret = (int32_t)(uint32_t)smc_ret.ret0;
	if (ret != SPCI_SUCCESS)
		return ret;

	if (x1 != NULL)
		*x1 = smc_ret.ret1;
	if (x2 != NULL)
		*x2 = smc_ret.ret2;
	if (x3 != NULL)
		*x3 = smc_ret.ret3;

	return SPCI_SUCCESS;
}

/* Returns a SPCI error code. On success, it also returns the returned values. */
int spci_service_request_blocking(u_register_t x1, u_register_t x2,
				  u_register_t x3, u_register_t x4,
				  u_register_t x5, u_register_t x6,
				  uint16_t client_id, uint16_t handle,
				  u_register_t *rx1, u_register_t *rx2,
				  u_register_t *rx3)
{
	int32_t ret;

	smc_args request_smc_args = {
		SPCI_SERVICE_REQUEST_BLOCKING_AARCH64,
		x1, x2, x3, x4, x5, x6,
		client_id | (handle << 16)
	};

	smc_ret_values smc_ret = tftf_smc(&request_smc_args);

	ret = (int32_t)(uint32_t)smc_ret.ret0;
	if (ret != SPCI_SUCCESS)
		return ret;

	if (rx1 != NULL)
		*rx1 = smc_ret.ret1;
	if (rx2 != NULL)
		*rx2 = smc_ret.ret2;
	if (rx3 != NULL)
		*rx3 = smc_ret.ret3;

	return SPCI_SUCCESS;
}
