/*
 * Copyright (c) 2018-2020, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>
#include <errno.h>

#include "cactus.h"
#include "cactus_def.h"
#include <cactus_platform_def.h>
#include "cactus_tests.h"
#include <debug.h>
#include <drivers/arm/pl011.h>
#include <drivers/console.h>
#include <ffa_helpers.h>
#include <lib/aarch64/arch_helpers.h>
#include <lib/xlat_tables/xlat_mmu_helpers.h>
#include <lib/xlat_tables/xlat_tables_v2.h>
#include <sp_helpers.h>
#include <std_svc.h>
#include <plat/common/platform.h>
#include <plat_arm.h>
#include <platform_def.h>

#include <cactus_test_cmds.h>

/* Host machine information injected by the build system in the ELF file. */
extern const char build_message[];
extern const char version_string[];

/* Memory section to be used for memory share operations */
static __aligned(PAGE_SIZE) uint8_t share_page[PAGE_SIZE];

/*
 *
 * Message loop function
 * Notice we cannot use regular print functions because this serves to both
 * "primary" and "secondary" VMs. Secondary VM cannot access UART directly
 * but rather through Hafnium print hypercall.
 *
 */
static void __dead2 message_loop(ffa_vm_id_t vm_id, struct mailbox_buffers *mb)
{
	smc_ret_values ffa_ret;
	uint32_t sp_response;
	ffa_vm_id_t source;
	ffa_vm_id_t destination;
	uint64_t cactus_cmd;

	/*
	* This initial wait call is necessary to inform SPMD that
	* SP initialization has completed. It blocks until receiving
	* a direct message request.
	*/

	ffa_ret = ffa_msg_wait();

	for (;;) {
		VERBOSE("Woke up with func id: %lx\n", ffa_ret.ret0);

		if (ffa_ret.ret0 == FFA_ERROR) {
			ERROR("Error: %lx\n", ffa_ret.ret2);
			break;
		}

		if (ffa_ret.ret0 != FFA_MSG_SEND_DIRECT_REQ_SMC32 &&
		    ffa_ret.ret0 != FFA_MSG_SEND_DIRECT_REQ_SMC64) {
			ERROR("%s(%u) unknown func id 0x%lx\n",
				__func__, vm_id, ffa_ret.ret0);
			break;
		}

		destination = ffa_ret.ret1 & U(0xFFFF);

		source = ffa_ret.ret1 >> 16;

		if (destination != vm_id) {
			ERROR("%s(%u) invalid vm id 0x%lx\n",
				__func__, vm_id, ffa_ret.ret1);
			break;
		}

		PRINT_CMD(ffa_ret);

		cactus_cmd = CACTUS_GET_CMD(ffa_ret);

		switch (cactus_cmd) {
		case FFA_MEM_SHARE_SMC32:
		case FFA_MEM_LEND_SMC32:
		case FFA_MEM_DONATE_SMC32:
			ffa_memory_management_test(
					mb, vm_id, source,
					CACTUS_GET_CMD(ffa_ret),
					CACTUS_MEM_SEND_GET_HANDLE(ffa_ret));

			/*
			 * If execution gets to this point means all operations
			 * with memory retrieval went well, as such replying
			 */
			ffa_ret = CACTUS_SUCCESS_RESP(vm_id, source);
			break;
		case CACTUS_REQ_MEM_SEND_CMD:
		{
			uint32_t mem_func =
				CACTUS_REQ_MEM_SEND_GET_MEM_FUNC(ffa_ret);
			ffa_vm_id_t receiver =
				CACTUS_REQ_MEM_SEND_GET_RECEIVER(ffa_ret);
			ffa_memory_handle_t handle;

			VERBOSE("%x requested to send memory to %x (func: %x)\n",
				source, receiver, mem_func);

			const struct ffa_memory_region_constituent
							constituents[] = {
				{(void *)share_page, 1, 0}
			};

			const uint32_t constituents_count = (
				sizeof(constituents) /
				sizeof(constituents[0])
			);

			handle = ffa_memory_init_and_send(
				(struct ffa_memory_region *)mb->send, PAGE_SIZE,
				vm_id, receiver, constituents,
				constituents_count, mem_func);

			/*
			 * If returned an invalid handle, we should break the
			 * test.
			 */
			expect(handle != FFA_MEMORY_HANDLE_INVALID, true);

			ffa_ret = CACTUS_MEM_SEND_CMD(vm_id, receiver, mem_func,
						      handle);

			if (ffa_ret.ret0 != FFA_MSG_SEND_DIRECT_RESP_SMC32) {
				ERROR("Failed to send message. error: %lx\n",
					ffa_ret.ret2);
				ffa_ret = CACTUS_ERROR_RESP(vm_id, source);
				break;
			}

			/* If anything went bad on the receiver's end. */
			if (CACTUS_IS_ERROR_RESP(ffa_ret)) {
				ERROR("Received error from receiver!\n");
				ffa_ret = CACTUS_ERROR_RESP(vm_id, source);
				break;
			}

			if (mem_func != FFA_MEM_DONATE_SMC32) {
				/*
				 * Do a memory reclaim only if the mem_func
				 * regards to memory share or lend operations,
				 * as with a donate the owner is permanently
				 * given up access to the memory region.
				 */
				if (ffa_mem_reclaim(handle, 0)
							.ret0 == FFA_ERROR) {
					ERROR("Failed to reclaim memory!\n");
					ffa_ret = CACTUS_ERROR_RESP(vm_id,
								    source);
					break;
				}

				/**
				 * Read Content that has been written to memory
				 * to validate access to memory segment has been
				 * reestablished, and receiver made use of
				 * memory region.
				 */
				#if (LOG_LEVEL >= LOG_LEVEL_VERBOSE)
					uint32_t *ptr =
						(uint32_t *)constituents
								->address;
					VERBOSE("Memory contents after receiver"
						" SP's use:\n");
					for (unsigned int i = 0U; i < 5U; i++)
						VERBOSE("      %u: %x\n", i,
									ptr[i]);
				#endif
			}

			ffa_ret = CACTUS_SUCCESS_RESP(vm_id, source);
		}
		break;
		case CACTUS_ECHO_CMD:
		{
			uint64_t echo_val = CACTUS_ECHO_GET_VAL(ffa_ret);

			VERBOSE("Received echo at %x, value %llx.\n",
				destination, echo_val);
			ffa_ret = CACTUS_RESPONSE(vm_id, source, echo_val);
			break;
		}
		case CACTUS_REQ_ECHO_CMD:
		{
			ffa_vm_id_t echo_dest =
					CACTUS_REQ_ECHO_GET_ECHO_DEST(ffa_ret);
			uint64_t echo_val = CACTUS_ECHO_GET_VAL(ffa_ret);
			bool success = true;

			VERBOSE("%x requested to send echo to %x, value %llx\n",
				source, echo_dest, echo_val);

			ffa_ret = CACTUS_ECHO_SEND_CMD(vm_id, echo_dest,
							echo_val);

			if (ffa_ret.ret0 != FFA_MSG_SEND_DIRECT_RESP_SMC32) {
				ERROR("Failed to send message. error: %lx\n",
					ffa_ret.ret2);
				success = false;
			}

			if (CACTUS_GET_RESPONSE(ffa_ret) != echo_val) {
				ERROR("Echo Failed!\n");
				success = false;
			}

			ffa_ret = success ? CACTUS_SUCCESS_RESP(vm_id, source) :
					    CACTUS_ERROR_RESP(vm_id, source);
			break;
		}
		case CACTUS_DEADLOCK_CMD:
		case CACTUS_REQ_DEADLOCK_CMD:
		{
			ffa_vm_id_t deadlock_dest =
				CACTUS_DEADLOCK_GET_NEXT_DEST(ffa_ret);
			ffa_vm_id_t deadlock_next_dest = source;

			if (cactus_cmd == CACTUS_DEADLOCK_CMD) {
				VERBOSE("%x is creating deadlock. next: %x\n",
					source, deadlock_dest);
			} else if (cactus_cmd == CACTUS_REQ_DEADLOCK_CMD) {
				VERBOSE(
				"%x requested deadlock with %x and %x\n",
				source, deadlock_dest, deadlock_next_dest);

				deadlock_next_dest =
					CACTUS_DEADLOCK_GET_NEXT_DEST2(ffa_ret);
			}

			ffa_ret = CACTUS_DEADLOCK_SEND_CMD(vm_id, deadlock_dest,
							   deadlock_next_dest);

			/*
			 * Should be true for the last partition to attempt
			 * an FF-A direct message, to the first partition.
			 */
			bool is_deadlock_detected =
				(ffa_ret.ret0 == FFA_ERROR) &&
				(ffa_ret.ret2 == FFA_ERROR_BUSY);

			/*
			 * Should be true after the deadlock has been detected
			 * and after the first response has been sent down the
			 * request chain.
			 */
			bool is_returning_from_deadlock =
				(ffa_ret.ret0 == FFA_MSG_SEND_DIRECT_RESP_SMC32)
				&& (CACTUS_IS_SUCCESS_RESP(ffa_ret));

			if (is_deadlock_detected) {
				NOTICE("Attempting dealock but got error %lx\n",
					ffa_ret.ret2);
			}

			if (is_deadlock_detected ||
			    is_returning_from_deadlock) {
				/*
				 * This is not the partition, that would have
				 * created the deadlock. As such, reply back
				 * to the partitions.
				 */
				ffa_ret = CACTUS_SUCCESS_RESP(vm_id, source);
				break;
			}

			/* Shouldn't get to this point */
			ERROR("Deadlock test went wrong!\n");
			ffa_ret = CACTUS_ERROR_RESP(vm_id, source);

			break;
		}
		default:
			/*
			 * Currently direct message test is handled here.
			 * TODO: create a case within the switch case
			 * For the sake of testing, add the vm id to the
			 * received message.
			 */
			sp_response = ffa_ret.ret3 | vm_id;
			VERBOSE("Replying with direct message response: %x\n", sp_response);
			ffa_ret = ffa_msg_send_direct_resp(vm_id,
							   HYP_ID,
							   sp_response);

			break;
		}
	}

	panic();
}

static const mmap_region_t cactus_mmap[] __attribute__((used)) = {
	/* PLAT_ARM_DEVICE0 area includes UART2 necessary to console */
	MAP_REGION_FLAT(PLAT_ARM_DEVICE0_BASE, PLAT_ARM_DEVICE0_SIZE,
			MT_DEVICE | MT_RW),
	{0}
};

static void cactus_print_memory_layout(unsigned int vm_id)
{
	INFO("Secure Partition memory layout:\n");

	INFO("  Text region            : %p - %p\n",
		(void *)CACTUS_TEXT_START, (void *)CACTUS_TEXT_END);

	INFO("  Read-only data region  : %p - %p\n",
		(void *)CACTUS_RODATA_START, (void *)CACTUS_RODATA_END);

	INFO("  Data region            : %p - %p\n",
		(void *)CACTUS_DATA_START, (void *)CACTUS_DATA_END);

	INFO("  BSS region             : %p - %p\n",
		(void *)CACTUS_BSS_START, (void *)CACTUS_BSS_END);

	INFO("  RX                     : %p - %p\n",
		(void *)get_sp_rx_start(vm_id),
		(void *)get_sp_rx_end(vm_id));

	INFO("  TX                     : %p - %p\n",
		(void *)get_sp_tx_start(vm_id),
		(void *)get_sp_tx_end(vm_id));
}

static void cactus_plat_configure_mmu(unsigned int vm_id)
{
	mmap_add_region(CACTUS_TEXT_START,
			CACTUS_TEXT_START,
			CACTUS_TEXT_END - CACTUS_TEXT_START,
			MT_CODE);
	mmap_add_region(CACTUS_RODATA_START,
			CACTUS_RODATA_START,
			CACTUS_RODATA_END - CACTUS_RODATA_START,
			MT_RO_DATA);
	mmap_add_region(CACTUS_DATA_START,
			CACTUS_DATA_START,
			CACTUS_DATA_END - CACTUS_DATA_START,
			MT_RW_DATA);
	mmap_add_region(CACTUS_BSS_START,
			CACTUS_BSS_START,
			CACTUS_BSS_END - CACTUS_BSS_START,
			MT_RW_DATA);

	mmap_add_region(get_sp_rx_start(vm_id),
			get_sp_rx_start(vm_id),
			(CACTUS_RX_TX_SIZE / 2),
			MT_RO_DATA);

	mmap_add_region(get_sp_tx_start(vm_id),
			get_sp_tx_start(vm_id),
			(CACTUS_RX_TX_SIZE / 2),
			MT_RW_DATA);

	mmap_add(cactus_mmap);
	init_xlat_tables();
}

int tftf_irq_handler_dispatcher(void)
{
	ERROR("%s\n", __func__);

	return 0;
}

void __dead2 cactus_main(void)
{
	assert(IS_IN_EL1() != 0);

	struct mailbox_buffers mb;

	/* Clear BSS */
	memset((void *)CACTUS_BSS_START,
	       0, CACTUS_BSS_END - CACTUS_BSS_START);

	/* Get current FFA id */
	smc_ret_values ffa_id_ret = ffa_id_get();
	if (ffa_id_ret.ret0 != FFA_SUCCESS_SMC32) {
		ERROR("FFA_ID_GET failed.\n");
		panic();
	}

	ffa_vm_id_t ffa_id = ffa_id_ret.ret2 & 0xffff;
	mb.send = (void *) get_sp_tx_start(ffa_id);
	mb.recv = (void *) get_sp_rx_start(ffa_id);

	/* Configure and enable Stage-1 MMU, enable D-Cache */
	cactus_plat_configure_mmu(ffa_id);
	enable_mmu_el1(0);

	if (ffa_id == SPM_VM_ID_FIRST) {
		console_init(CACTUS_PL011_UART_BASE,
			     CACTUS_PL011_UART_CLK_IN_HZ,
			     PL011_BAUDRATE);

		set_putc_impl(PL011_AS_STDOUT);

		NOTICE("Booting Primary Cactus Secure Partition\n%s\n%s\n",
			build_message, version_string);
	} else {
		smc_ret_values ret;
		set_putc_impl(HVC_CALL_AS_STDOUT);

		NOTICE("Booting Secondary Cactus Secure Partition (ID: %x)\n%s\n%s\n",
			ffa_id, build_message, version_string);

		if (ffa_id == (SPM_VM_ID_FIRST + 2)) {
			VERBOSE("Mapping RXTX Region\n");
			CONFIGURE_AND_MAP_MAILBOX(mb, PAGE_SIZE, ret);
			if (ret.ret0 != FFA_SUCCESS_SMC32) {
				ERROR(
				    "Failed to map RXTX buffers. Error: %lx\n",
				    ret.ret2);
				panic();
			}
		}
	}

	INFO("FF-A id: %x\n", ffa_id);
	cactus_print_memory_layout(ffa_id);

	/* Invoking Tests */
	ffa_tests(&mb);

	/* End up to message loop */
	message_loop(ffa_id, &mb);

	/* Not reached */
}
