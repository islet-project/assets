/*
 * Copyright (c) 2021, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef CACTUS_TEST_CMDS
#define CACTUS_TEST_CMDS

#include <ffa_helpers.h>
#include <spm_common.h>

/**
 * Success and error return to be sent over a msg response.
 */
#define CACTUS_SUCCESS		U(0)
#define CACTUS_ERROR		U(-1)

/**
 * Error codes.
 */
#define CACTUS_ERROR_INVALID		U(1)
#define CACTUS_ERROR_TEST		U(2)
#define CACTUS_ERROR_FFA_CALL		U(3)
#define CACTUS_ERROR_UNHANDLED		U(4)

/**
 * Get command from struct smc_ret_values.
 */
static inline uint64_t cactus_get_cmd(smc_ret_values ret)
{
	return (uint64_t)ret.ret3;
}

/**
 * Template for commands to be sent to CACTUS partitions over direct
 * messages interfaces.
 */
static inline smc_ret_values cactus_send_cmd(
	ffa_id_t source, ffa_id_t dest, uint64_t cmd, uint64_t val0,
	uint64_t val1, uint64_t val2, uint64_t val3)
{
	return 	ffa_msg_send_direct_req64(source, dest, cmd, val0, val1, val2,
					  val3);
}

/**
 * Template for responses to Cactus commands.
 * 'cactus_send_response' is the template for custom responses, in case there is
 * a need to propagate more than one value in the response of a command.
 */
static inline smc_ret_values cactus_send_response(
	ffa_id_t source, ffa_id_t dest, uint32_t resp, uint64_t val0,
	uint64_t val1, uint64_t val2, uint64_t val3)
{
	return ffa_msg_send_direct_resp64(source, dest, resp, val0, val1,
					  val2, val3);
}

/**
 * For responses of one value only.
 */
static inline smc_ret_values cactus_response(
	ffa_id_t source, ffa_id_t dest, uint32_t response)
{
	return ffa_msg_send_direct_resp64(source, dest, response, 0, 0, 0, 0);
}

static inline uint32_t cactus_get_response(smc_ret_values ret)
{
	return (uint32_t)ret.ret3;
}

/**
 * In a successful test, in case the SP needs to propagate an extra value
 * to conclude the test.
 * If more arguments are needed, a custom response should be defined for the
 * specific test.
 */
static inline smc_ret_values cactus_success_resp(
		ffa_id_t source, ffa_id_t dest, uint64_t value)
{
	return cactus_send_response(source, dest, CACTUS_SUCCESS, value,
				    0, 0, 0);
}

/**
 * In case the test fails on the SP side, the 'error_code' should help specify
 * the reason, which can be specific to the test, or general ones as defined
 * in the error code list.
 */
static inline smc_ret_values cactus_error_resp(
		ffa_id_t source, ffa_id_t dest, uint32_t error_code)
{
	return cactus_send_response(source, dest, CACTUS_ERROR, error_code,
				    0, 0, 0);
}

static inline uint32_t cactus_error_code(smc_ret_values ret)
{
	return (uint32_t) ret.ret4;
}

/**
 * With this test command the sender transmits a 64-bit value that it then
 * expects to receive on the respective command response.
 *
 * The id is the hex representation of the string 'echo'.
 */
#define CACTUS_ECHO_CMD U(0x6563686f)

static inline smc_ret_values cactus_echo_send_cmd(
	ffa_id_t source, ffa_id_t dest, uint64_t echo_val)
{
	return cactus_send_cmd(source, dest, CACTUS_ECHO_CMD, echo_val, 0, 0,
			       0);
}

static inline uint64_t cactus_echo_get_val(smc_ret_values ret)
{
	return (uint64_t)ret.ret4;
}

/**
 * Command to request a cactus secure partition to send an echo command to
 * another partition.
 *
 * The sender of this command expects to receive CACTUS_SUCCESS if the requested
 * echo interaction happened successfully, or CACTUS_ERROR otherwise.
 */
#define CACTUS_REQ_ECHO_CMD (CACTUS_ECHO_CMD + 1)

static inline smc_ret_values cactus_req_echo_send_cmd(
	ffa_id_t source, ffa_id_t dest, ffa_id_t echo_dest,
	uint64_t echo_val)
{
	return cactus_send_cmd(source, dest, CACTUS_REQ_ECHO_CMD, echo_val,
			       echo_dest, 0, 0);
}

static inline ffa_id_t cactus_req_echo_get_echo_dest(smc_ret_values ret)
{
	return (ffa_id_t)ret.ret5;
}

/**
 * Command to create a cyclic dependency between SPs, which could result in
 * a deadlock. This aims at proving such scenario cannot happen.
 * If the deadlock happens, the system will just hang.
 * If the deadlock is prevented, the last partition to use the command will
 * send response CACTUS_SUCCESS.
 *
 * The id is the hex representation of the string 'dead'.
 */
#define CACTUS_DEADLOCK_CMD U(0x64656164)

static inline smc_ret_values cactus_deadlock_send_cmd(
	ffa_id_t source, ffa_id_t dest, ffa_id_t next_dest)
{
	return cactus_send_cmd(source, dest, CACTUS_DEADLOCK_CMD, next_dest, 0,
			       0, 0);
}

static inline ffa_id_t cactus_deadlock_get_next_dest(smc_ret_values ret)
{
	return (ffa_id_t)ret.ret4;
}

/**
 * Command to request a sequence CACTUS_DEADLOCK_CMD between the partitions
 * of specified IDs.
 */
#define CACTUS_REQ_DEADLOCK_CMD (CACTUS_DEADLOCK_CMD + 1)

static inline smc_ret_values cactus_req_deadlock_send_cmd(
	ffa_id_t source, ffa_id_t dest, ffa_id_t next_dest1,
	ffa_id_t next_dest2)
{
	return cactus_send_cmd(source, dest, CACTUS_REQ_DEADLOCK_CMD,
			       next_dest1, next_dest2, 0, 0);
}

/* To get next_dest1 use CACTUS_DEADLOCK_GET_NEXT_DEST */
static inline ffa_id_t cactus_deadlock_get_next_dest2(smc_ret_values ret)
{
	return (ffa_id_t)ret.ret5;
}

/**
 * Command to notify cactus of a memory management operation. The cmd value
 * should be the memory management smc function id.
 *
 * The id is the hex representation of the string "mem"
 */
#define CACTUS_MEM_SEND_CMD U(0x6d656d)

static inline smc_ret_values cactus_mem_send_cmd(
	ffa_id_t source, ffa_id_t dest, uint32_t mem_func,
	ffa_memory_handle_t handle)
{
	return cactus_send_cmd(source, dest, CACTUS_MEM_SEND_CMD, mem_func,
			       handle, 0, 0);
}

static inline ffa_memory_handle_t cactus_mem_send_get_handle(smc_ret_values ret)
{
	return (ffa_memory_handle_t)ret.ret5;
}

/**
 * Command to request a memory management operation. The 'mem_func' argument
 * identifies the operation that is to be performend, and 'receiver' is the id
 * of the partition to receive the memory region.
 *
 * The command id is the hex representation of the string "memory".
 */
#define CACTUS_REQ_MEM_SEND_CMD U(0x6d656d6f7279)

static inline smc_ret_values cactus_req_mem_send_send_cmd(
	ffa_id_t source, ffa_id_t dest, uint32_t mem_func,
	ffa_id_t receiver)
{
	return cactus_send_cmd(source, dest, CACTUS_REQ_MEM_SEND_CMD, mem_func,
			       receiver, 0, 0);
}

static inline uint32_t cactus_req_mem_send_get_mem_func(smc_ret_values ret)
{
	return (uint32_t)ret.ret4;
}

static inline ffa_id_t cactus_req_mem_send_get_receiver(smc_ret_values ret)
{
	return (ffa_id_t)ret.ret5;
}

/**
 * Request to fill SIMD vectors with dummy values with purpose to check a
 * save/restore routine during the context switches between secure world and
 * normal world.
 *
 * The command id is the hex representation of the string "SIMD"
 */
#define CACTUS_REQ_SIMD_FILL_CMD U(0x53494d44)

static inline smc_ret_values cactus_req_simd_fill_send_cmd(
	ffa_id_t source, ffa_id_t dest)
{
	return cactus_send_cmd(source, dest, CACTUS_REQ_SIMD_FILL_CMD, 0, 0, 0,
			       0);
}

/**
 * Command to request cactus to sleep for the given time in ms
 *
 * The command id is the hex representation of string "sleep"
 */
#define CACTUS_SLEEP_CMD U(0x736c656570)

static inline smc_ret_values cactus_sleep_cmd(
	ffa_id_t source, ffa_id_t dest, uint32_t sleep_time)
{
	return cactus_send_cmd(source, dest, CACTUS_SLEEP_CMD, sleep_time, 0, 0,
			       0);
}

static inline uint32_t cactus_get_sleep_time(smc_ret_values ret)
{
	return (uint32_t)ret.ret4;
}

/**
 * Command to request cactus to enable/disable an interrupt
 *
 * The command id is the hex representation of string "intr"
 */
#define CACTUS_INTERRUPT_CMD U(0x696e7472)

static inline smc_ret_values cactus_interrupt_cmd(
	ffa_id_t source, ffa_id_t dest, uint32_t interrupt_id,
	bool enable, uint32_t pin)
{
	return cactus_send_cmd(source, dest, CACTUS_INTERRUPT_CMD, interrupt_id,
			       enable, pin, 0);
}

static inline uint32_t cactus_get_interrupt_id(smc_ret_values ret)
{
	return (uint32_t)ret.ret4;
}

static inline bool cactus_get_interrupt_enable(smc_ret_values ret)
{
	return (bool)ret.ret5;
}

static inline enum interrupt_pin cactus_get_interrupt_pin(smc_ret_values ret)
{
	return (enum interrupt_pin)ret.ret6;
}

/**
 * Request to initiate DMA transaction by upstream peripheral.
 *
 * The command id is the hex representation of the string "SMMU"
 */
#define CACTUS_DMA_SMMUv3_CMD           (0x534d4d55)

static inline smc_ret_values cactus_send_dma_cmd(
	ffa_id_t source, ffa_id_t dest)
{
	return cactus_send_cmd(source, dest, CACTUS_DMA_SMMUv3_CMD, 0, 0, 0,
			       0);
}
#endif
