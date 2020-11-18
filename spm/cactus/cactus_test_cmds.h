/*
 * Copyright (c) 2020, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef CACTUS_TEST_CMDS
#define CACTUS_TEST_CMDS

#include <debug.h>
#include <ffa_helpers.h>

/**
 * Success and error return to be sent over a msg response.
 */
#define CACTUS_SUCCESS	 U(0)
#define CACTUS_ERROR	 U(-1)

/**
 * Get command from struct smc_ret_values.
 */
#define CACTUS_GET_CMD(smc_ret) smc_ret.ret3

/**
 * Template for commands to be sent to CACTUS partitions over direct
 * messages interfaces.
 */
#define CACTUS_SEND_CMD(source, dest, cmd, val0, val1, val2, val3)	\
	ffa_msg_send_direct_req64_5args(source, dest, cmd, 		\
					val0, val1, val2, val3)

#define PRINT_CMD(smc_ret)						\
	VERBOSE("cmd %lx; args: %lx, %lx, %lx, %lx\n",	 		\
		smc_ret.ret3, smc_ret.ret4, smc_ret.ret5, 		\
		smc_ret.ret6, smc_ret.ret7)

/**
 * With this test command the sender transmits a 64-bit value that it then
 * expects to receive on the respective command response.
 *
 * The id is the hex representation of the string 'echo'.
 */
#define CACTUS_ECHO_CMD U(0x6563686f)

#define CACTUS_ECHO_SEND_CMD(source, dest, echo_val) 			\
		CACTUS_SEND_CMD(source, dest, CACTUS_ECHO_CMD, echo_val,\
				0, 0, 0)

#define CACTUS_ECHO_GET_VAL(smc_ret) smc_ret.ret4

/**
 * Command to request a cactus secure partition to send an echo command to
 * another partition.
 *
 * The sender of this command expects to receive CACTUS_SUCCESS if the requested
 * echo interaction happened successfully, or CACTUS_ERROR otherwise.
 */
#define CACTUS_REQ_ECHO_CMD (CACTUS_ECHO_CMD + 1)

#define CACTUS_REQ_ECHO_SEND_CMD(source, dest, echo_dest, echo_val) 	\
		CACTUS_SEND_CMD(source, dest, CACTUS_REQ_ECHO_CMD, echo_val, \
				echo_dest, 0, 0)

#define CACTUS_REQ_ECHO_GET_ECHO_DEST(smc_ret) smc_ret.ret5

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

#define CACTUS_DEADLOCK_SEND_CMD(source, dest, next_dest)		\
		CACTUS_SEND_CMD(source, dest, CACTUS_DEADLOCK_CMD, next_dest, \
				0, 0, 0)

#define CACTUS_DEADLOCK_GET_NEXT_DEST(smc_ret) smc_ret.ret4

/**
 * Command to request a sequence CACTUS_DEADLOCK_CMD between the partitions
 * of specified IDs.
 */
#define CACTUS_REQ_DEADLOCK_CMD (CACTUS_DEADLOCK_CMD + 1)

#define CACTUS_REQ_DEADLOCK_SEND_CMD(source, dest, next_dest1, next_dest2)  \
		CACTUS_SEND_CMD(source, dest, CACTUS_REQ_DEADLOCK_CMD,	 \
				next_dest1, next_dest2, 0, 0)

/*To get next_dest1 use CACTUS_DEADLOCK_GET_NEXT_DEST*/
#define CACTUS_DEADLOCK_GET_NEXT_DEST2(smc_ret) smc_ret.ret5

/**
 * Command to notify cactus of a memory management operation. The cmd value
 * should be the memory management smc function id.
 */
#define CACTUS_MEM_SEND_CMD(source, dest, mem_func, handle) 			\
		CACTUS_SEND_CMD(source, dest, mem_func, handle, 0, 0, 0)

#define CACTUS_MEM_SEND_GET_HANDLE(smc_ret) smc_ret.ret4

/**
 * Command to request a memory management operation. The 'mem_func' argument
 * identifies the operation that is to be performend, and 'receiver' is the id
 * of the partition to receive the memory region.
 *
 * The command id is the hex representation of the string "memory".
 */
#define CACTUS_REQ_MEM_SEND_CMD U(0x6d656d6f7279)

#define CACTUS_REQ_MEM_SEND_SEND_CMD(source, dest, mem_func, receiver)		\
	CACTUS_SEND_CMD(source, dest, CACTUS_REQ_MEM_SEND_CMD, mem_func,	\
			receiver, 0, 0)

#define CACTUS_REQ_MEM_SEND_GET_MEM_FUNC(smc_ret) smc_ret.ret4
#define CACTUS_REQ_MEM_SEND_GET_RECEIVER(smc_ret) smc_ret.ret5

/**
 * Template for responses to CACTUS commands.
 */
#define CACTUS_RESPONSE(source, dest, response) 				\
		ffa_msg_send_direct_resp(source, dest, response)

#define CACTUS_SUCCESS_RESP(source, dest) 					\
		CACTUS_RESPONSE(source, dest, CACTUS_SUCCESS)

#define CACTUS_ERROR_RESP(source, dest) 					\
		CACTUS_RESPONSE(source, dest, CACTUS_ERROR)

#define CACTUS_GET_RESPONSE(smc_ret) smc_ret.ret3

#define CACTUS_IS_SUCCESS_RESP(smc_ret)					\
		(CACTUS_GET_RESPONSE(smc_ret) == CACTUS_SUCCESS)

#define CACTUS_IS_ERROR_RESP(smc_ret)						\
		(CACTUS_GET_RESPONSE(smc_ret) == CACTUS_ERROR)

#endif
