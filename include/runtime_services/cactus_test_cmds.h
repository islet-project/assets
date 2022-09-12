/*
 * Copyright (c) 2021-2022, Arm Limited. All rights reserved.
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

#define ECHO_VAL1 U(0xa0a0a0a0)
#define ECHO_VAL2 U(0xb0b0b0b0)
#define ECHO_VAL3 U(0xc0c0c0c0)

/**
 * Get command from struct ffa_value.
 */
static inline uint64_t cactus_get_cmd(struct ffa_value ret)
{
	return (uint64_t)ret.arg3;
}

/**
 * Template for commands to be sent to CACTUS partitions over direct
 * messages interfaces.
 */
static inline struct ffa_value cactus_send_cmd(
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
static inline struct ffa_value cactus_send_response(
	ffa_id_t source, ffa_id_t dest, uint32_t resp, uint64_t val0,
	uint64_t val1, uint64_t val2, uint64_t val3)
{
	return ffa_msg_send_direct_resp64(source, dest, resp, val0, val1,
					  val2, val3);
}

/**
 * For responses of one value only.
 */
static inline struct ffa_value cactus_response(
	ffa_id_t source, ffa_id_t dest, uint32_t response)
{
	return cactus_send_response(source, dest, response, 0, 0, 0, 0);
}

static inline uint32_t cactus_get_response(struct ffa_value ret)
{
	return (uint32_t)ret.arg3;
}

/**
 * In a successful test, in case the SP needs to propagate an extra value
 * to conclude the test.
 * If more arguments are needed, a custom response should be defined for the
 * specific test.
 */
static inline struct ffa_value cactus_success_resp(
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
static inline struct ffa_value cactus_error_resp(
		ffa_id_t source, ffa_id_t dest, uint32_t error_code)
{
	return cactus_send_response(source, dest, CACTUS_ERROR, error_code,
				    0, 0, 0);
}

static inline uint32_t cactus_error_code(struct ffa_value ret)
{
	return (uint32_t) ret.arg4;
}

/**
 * With this test command the sender transmits a 64-bit value that it then
 * expects to receive on the respective command response.
 *
 * The id is the hex representation of the string 'echo'.
 */
#define CACTUS_ECHO_CMD U(0x6563686f)

static inline struct ffa_value cactus_echo_send_cmd(
	ffa_id_t source, ffa_id_t dest, uint64_t echo_val)
{
	return cactus_send_cmd(source, dest, CACTUS_ECHO_CMD, echo_val, 0, 0,
			       0);
}

static inline uint64_t cactus_echo_get_val(struct ffa_value ret)
{
	return (uint64_t)ret.arg4;
}

/**
 * Command to request a cactus secure partition to send an echo command to
 * another partition.
 *
 * The sender of this command expects to receive CACTUS_SUCCESS if the requested
 * echo interaction happened successfully, or CACTUS_ERROR otherwise.
 */
#define CACTUS_REQ_ECHO_CMD (CACTUS_ECHO_CMD + 1)

static inline struct ffa_value cactus_req_echo_send_cmd(
	ffa_id_t source, ffa_id_t dest, ffa_id_t echo_dest,
	uint64_t echo_val)
{
	return cactus_send_cmd(source, dest, CACTUS_REQ_ECHO_CMD, echo_val,
			       echo_dest, 0, 0);
}

static inline ffa_id_t cactus_req_echo_get_echo_dest(struct ffa_value ret)
{
	return (ffa_id_t)ret.arg5;
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

static inline struct ffa_value cactus_deadlock_send_cmd(
	ffa_id_t source, ffa_id_t dest, ffa_id_t next_dest)
{
	return cactus_send_cmd(source, dest, CACTUS_DEADLOCK_CMD, next_dest, 0,
			       0, 0);
}

static inline ffa_id_t cactus_deadlock_get_next_dest(struct ffa_value ret)
{
	return (ffa_id_t)ret.arg4;
}

/**
 * Command to request a sequence CACTUS_DEADLOCK_CMD between the partitions
 * of specified IDs.
 */
#define CACTUS_REQ_DEADLOCK_CMD (CACTUS_DEADLOCK_CMD + 1)

static inline struct ffa_value cactus_req_deadlock_send_cmd(
	ffa_id_t source, ffa_id_t dest, ffa_id_t next_dest1,
	ffa_id_t next_dest2)
{
	return cactus_send_cmd(source, dest, CACTUS_REQ_DEADLOCK_CMD,
			       next_dest1, next_dest2, 0, 0);
}

/* To get next_dest1 use CACTUS_DEADLOCK_GET_NEXT_DEST */
static inline ffa_id_t cactus_deadlock_get_next_dest2(struct ffa_value ret)
{
	return (ffa_id_t)ret.arg5;
}

/**
 * Command to notify cactus of a memory management operation. The cmd value
 * should be the memory management smc function id.
 *
 * The id is the hex representation of the string "mem"
 */
#define CACTUS_MEM_SEND_CMD U(0x6d656d)

static inline struct ffa_value cactus_mem_send_cmd(
	ffa_id_t source, ffa_id_t dest, uint32_t mem_func,
	ffa_memory_handle_t handle, ffa_memory_region_flags_t retrieve_flags,
	bool non_secure, uint16_t word_to_write)
{
	/*
	 * `non_secure` and `word_to_write` are packed in the same register.
	 * Packed in a 32-bit value to support AArch32 platforms (eg Juno).
	 */
	uint32_t val3 = ((uint32_t)non_secure << 16) | word_to_write;
	return cactus_send_cmd(source, dest, CACTUS_MEM_SEND_CMD, mem_func,
			       handle, retrieve_flags, val3);
}

static inline ffa_memory_handle_t cactus_mem_send_get_handle(
	struct ffa_value ret)
{
	return (ffa_memory_handle_t)ret.arg5;
}

static inline ffa_memory_region_flags_t cactus_mem_send_get_retrv_flags(
	struct ffa_value ret)
{
	return (ffa_memory_region_flags_t)ret.arg6;
}

static inline uint16_t cactus_mem_send_words_to_write(struct ffa_value ret)
{
	return (uint16_t)ret.arg7;
}

static inline bool cactus_mem_send_get_non_secure(struct ffa_value ret)
{
	return (bool)(ret.arg7 >> 16);
}

/**
 * Command to request a memory management operation. The 'mem_func' argument
 * identifies the operation that is to be performend, and 'receiver' is the id
 * of the partition to receive the memory region.
 *
 * The command id is the hex representation of the string "memory".
 */
#define CACTUS_REQ_MEM_SEND_CMD U(0x6d656d6f7279)

static inline struct ffa_value cactus_req_mem_send_send_cmd(
	ffa_id_t source, ffa_id_t dest, uint32_t mem_func,
	ffa_id_t receiver, bool non_secure)
{
	return cactus_send_cmd(source, dest, CACTUS_REQ_MEM_SEND_CMD, mem_func,
			       receiver, non_secure, 0);
}

static inline uint32_t cactus_req_mem_send_get_mem_func(struct ffa_value ret)
{
	return (uint32_t)ret.arg4;
}

static inline ffa_id_t cactus_req_mem_send_get_receiver(struct ffa_value ret)
{
	return (ffa_id_t)ret.arg5;
}

static inline bool cactus_req_mem_send_get_non_secure(struct ffa_value ret)
{
	return (bool)ret.arg6;
}

/**
 * Request to fill SIMD vectors with dummy values with purpose to check a
 * save/restore routine during the context switches between secure world and
 * normal world.
 *
 * The command id is the hex representation of the string "SIMD"
 */
#define CACTUS_REQ_SIMD_FILL_CMD U(0x53494d44)

static inline struct ffa_value cactus_req_simd_fill_send_cmd(
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

static inline struct ffa_value cactus_sleep_cmd(
	ffa_id_t source, ffa_id_t dest, uint32_t sleep_time)
{
	return cactus_send_cmd(source, dest, CACTUS_SLEEP_CMD, sleep_time, 0, 0,
			       0);
}

/**
 * Command to request cactus to forward sleep command for the given time in ms
 *
 * The sender of this command expects to receive CACTUS_SUCCESS if the requested
 * echo interaction happened successfully, or CACTUS_ERROR otherwise.
 * Moreover, the sender can send a hint to the destination SP to expect that
 * the forwaded sleep command could be preempted by a non-secure interrupt.
 */
#define CACTUS_FWD_SLEEP_CMD (CACTUS_SLEEP_CMD + 1)

static inline struct ffa_value cactus_fwd_sleep_cmd(
	ffa_id_t source, ffa_id_t dest, ffa_id_t fwd_dest,
	uint32_t sleep_time, bool hint_interrupted)
{
	return cactus_send_cmd(source, dest, CACTUS_FWD_SLEEP_CMD, sleep_time,
			       fwd_dest, hint_interrupted, 0);
}

static inline uint32_t cactus_get_sleep_time(struct ffa_value ret)
{
	return (uint32_t)ret.arg4;
}

static inline ffa_id_t cactus_get_fwd_sleep_dest(struct ffa_value ret)
{
	return (ffa_id_t)ret.arg5;
}

static inline bool cactus_get_fwd_sleep_interrupted_hint(struct ffa_value ret)
{
	return (bool)ret.arg6;
}

/**
 * Command to request cactus to sleep for half the given time in ms, trigger
 * trusted watchdog timer and then sleep again for another half the given time.
 *
 * The sender of this command expects to receive CACTUS_SUCCESS if the requested
 * echo interaction happened successfully, or CACTUS_ERROR otherwise.
 */
#define CACTUS_SLEEP_TRIGGER_TWDOG_CMD (CACTUS_SLEEP_CMD + 2)

static inline struct ffa_value cactus_sleep_trigger_wdog_cmd(
	ffa_id_t source, ffa_id_t dest, uint32_t sleep_time,
	uint64_t wdog_time)
{
	return cactus_send_cmd(source, dest, CACTUS_SLEEP_TRIGGER_TWDOG_CMD, sleep_time,
			       wdog_time, 0, 0);
}


static inline uint32_t cactus_get_wdog_trigger_duration(struct ffa_value ret)
{
	return (uint32_t)ret.arg5;
}

/**
 * Command to request cactus to enable/disable an interrupt
 *
 * The command id is the hex representation of string "intr"
 */
#define CACTUS_INTERRUPT_CMD U(0x696e7472)

static inline struct ffa_value cactus_interrupt_cmd(
	ffa_id_t source, ffa_id_t dest, uint32_t interrupt_id,
	bool enable, uint32_t pin)
{
	return cactus_send_cmd(source, dest, CACTUS_INTERRUPT_CMD, interrupt_id,
			       enable, pin, 0);
}

static inline uint32_t cactus_get_interrupt_id(struct ffa_value ret)
{
	return (uint32_t)ret.arg4;
}

static inline bool cactus_get_interrupt_enable(struct ffa_value ret)
{
	return (bool)ret.arg5;
}

static inline enum interrupt_pin cactus_get_interrupt_pin(struct ffa_value ret)
{
	return (enum interrupt_pin)ret.arg6;
}

/**
 * Request to initiate DMA transaction by upstream peripheral.
 *
 * The command id is the hex representation of the string "SMMU"
 */
#define CACTUS_DMA_SMMUv3_CMD           (0x534d4d55)

static inline struct ffa_value cactus_send_dma_cmd(
	ffa_id_t source, ffa_id_t dest)
{
	return cactus_send_cmd(source, dest, CACTUS_DMA_SMMUv3_CMD, 0, 0, 0,
			       0);
}

/*
 * Request SP to bind a notification to a FF-A endpoint. In case of error
 * when using the FFA_NOTIFICATION_BIND interface, include the error code
 * in the response to the command's request. The receiver and sender arguments
 * are propagated through the command's arguments, to allow the test of
 * erroneous uses of the FFA_NOTIFICATION_BIND interface.
 *
 * The command id is the hex representation of the string "bind".
 */
#define CACTUS_NOTIFICATION_BIND_CMD U(0x62696e64)

static inline struct ffa_value cactus_notification_bind_send_cmd(
	ffa_id_t source, ffa_id_t dest, ffa_id_t receiver,
	ffa_id_t sender, ffa_notification_bitmap_t notifications, uint32_t flags)
{
	return cactus_send_cmd(source, dest, CACTUS_NOTIFICATION_BIND_CMD,
			       receiver, sender, notifications, flags);
}

/**
 * Request to SP unbind a notification. In case of error when using the
 * FFA_NOTIFICATION_UNBIND interface, the test includes the error code in the
 * response. The receiver and sender arguments are propagated throught the
 * command's arguments, to allow the test of erroneous uses of the
 * FFA_NOTIFICATION_BIND interface.
 *
 * The command id is the hex representation of the string "unbind".
 */
#define CACTUS_NOTIFICATION_UNBIND_CMD U(0x756e62696e64)

static inline struct ffa_value cactus_notification_unbind_send_cmd(
	ffa_id_t source, ffa_id_t dest, ffa_id_t receiver,
	ffa_id_t sender, ffa_notification_bitmap_t notifications)
{
	return cactus_send_cmd(source, dest, CACTUS_NOTIFICATION_UNBIND_CMD,
			       receiver, sender, notifications, 0);
}

static inline ffa_id_t cactus_notification_get_receiver(struct ffa_value ret)
{
	return (ffa_id_t)ret.arg4;
}

static inline ffa_id_t cactus_notification_get_sender(struct ffa_value ret)
{
	return (ffa_id_t)ret.arg5;
}

static inline ffa_notification_bitmap_t cactus_notification_get_notifications(
	struct ffa_value ret)
{
	return (uint64_t)ret.arg6;
}

/**
 * Request SP to get notifications. The arguments to use in ffa_notification_get
 * are propagated on the command to test erroneous uses of the interface.
 * In a successful call to the interface, the SP's response payload should
 * include all bitmaps returned by the SPMC.
 *
 * The command id is the hex representation of the string "getnot".
 */
#define CACTUS_NOTIFICATION_GET_CMD U(0x6765746e6f74)

static inline struct ffa_value cactus_notification_get_send_cmd(
	ffa_id_t source, ffa_id_t dest, ffa_id_t receiver,
	uint32_t vcpu_id, uint32_t flags, bool check_npi_handled)
{
	return cactus_send_cmd(source, dest, CACTUS_NOTIFICATION_GET_CMD,
			       receiver, vcpu_id, check_npi_handled, flags);
}

static inline uint32_t cactus_notification_get_vcpu(struct ffa_value ret)
{
	return (uint32_t)ret.arg5;
}

static inline uint32_t cactus_notification_get_flags(struct ffa_value ret)
{
	return (uint32_t)ret.arg7;
}

static inline struct ffa_value cactus_notifications_get_success_resp(
	ffa_id_t source, ffa_id_t dest, uint64_t from_sp,
	uint64_t from_vm)
{
	return cactus_send_response(source, dest, CACTUS_SUCCESS, from_sp,
				    from_vm, 0, 0);
}

static inline uint64_t cactus_notifications_get_from_sp(struct ffa_value ret)
{
	return (uint64_t)ret.arg4;
}

static inline uint64_t cactus_notifications_get_from_vm(struct ffa_value ret)
{
	return (uint64_t)ret.arg5;
}

static inline bool cactus_notifications_check_npi_handled(struct ffa_value ret)
{
	return (bool)ret.arg6;
}

/**
 * Request SP to set notifications. The arguments to use in ffa_notification_set
 * are propagated on the command to test erroneous uses of the interface.
 * In case of error while calling the interface, the response should include the
 * error code. If in the flags a delay SRI is requested, cactus should
 * send a CACTUS_ECHO_CMD to the SP specified as `echo_dest`. This should help
 * validate that the SRI is only sent when returning execution to the NWd.
 */
#define CACTUS_NOTIFICATIONS_SET_CMD U(0x6e6f74736574)

static inline struct ffa_value cactus_notifications_set_send_cmd(
	ffa_id_t source, ffa_id_t dest, ffa_id_t receiver,
	ffa_id_t sender, uint32_t flags, ffa_notification_bitmap_t notifications,
	ffa_id_t echo_dest)
{
	return cactus_send_cmd(source, dest, CACTUS_NOTIFICATIONS_SET_CMD,
			       (uint32_t)receiver | ((uint32_t)sender << 16),
			       echo_dest,
			       notifications, flags);
}

static inline ffa_id_t cactus_notifications_set_get_receiver(
	struct ffa_value ret)
{
	return (ffa_id_t)(ret.arg4 & 0xFFFFU);
}

static inline ffa_id_t cactus_notifications_set_get_sender(struct ffa_value ret)
{
	return (ffa_id_t)((ret.arg4 >> 16U) & 0xFFFFU);
}

/**
 * Request to start trusted watchdog timer.
 *
 * The command id is the hex representaton of the string "WDOG"
 */
#define CACTUS_TWDOG_START_CMD		U(0x57444f47)

static inline struct ffa_value cactus_send_twdog_cmd(
	ffa_id_t source, ffa_id_t dest, uint64_t time)
{
	return cactus_send_cmd(source, dest, CACTUS_TWDOG_START_CMD, time, 0, 0,
			       0);
}

static inline uint32_t cactus_get_wdog_duration(struct ffa_value ret)
{
	return (uint32_t)ret.arg4;
}

/**
 * Request SP to return the current count of handled requests.
 *
 * The command id is the hex representation of the string "getnot".
 */
#define CACTUS_GET_REQ_COUNT_CMD U(0x726571636f756e74)

static inline struct ffa_value cactus_get_req_count_send_cmd(
	ffa_id_t source, ffa_id_t dest)
{
	return cactus_send_cmd(source, dest, CACTUS_GET_REQ_COUNT_CMD, 0, 0, 0,
			       0);
}

static inline uint32_t cactus_get_req_count(struct ffa_value ret)
{
	return (uint32_t)ret.arg4;
}

/**
 * Request SP to return the last serviced secure virtual interrupt.
 *
 * The command id is the hex representaton of the string "vINT"
 */
#define CACTUS_LAST_INTERRUPT_SERVICED_CMD U(0x76494e54)

static inline struct ffa_value cactus_get_last_interrupt_cmd(
	ffa_id_t source, ffa_id_t dest)
{
	return cactus_send_cmd(source, dest, CACTUS_LAST_INTERRUPT_SERVICED_CMD,
				 0, 0, 0, 0);
}
#endif
