/*
 * Copyright (c) 2018-2021, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>
#include <errno.h>
#include <debug.h>

#include <cactus_platform_def.h>
#include <cactus_test_cmds.h>
#include <drivers/arm/pl011.h>
#include <drivers/console.h>
#include <lib/aarch64/arch_helpers.h>
#include <lib/xlat_tables/xlat_mmu_helpers.h>
#include <lib/xlat_tables/xlat_tables_v2.h>
#include <plat_arm.h>
#include <plat/common/platform.h>
#include <platform_def.h>
#include <sp_helpers.h>
#include <std_svc.h>

#include "cactus_def.h"
#include "cactus_tests.h"
#include "cactus.h"

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
		VERBOSE("Woke up with func id: %x\n", ffa_func_id(ffa_ret));

		if (ffa_func_id(ffa_ret) == FFA_ERROR) {
			ERROR("Error: %x\n", ffa_error_code(ffa_ret));
			break;
		}

		if (ffa_func_id(ffa_ret) != FFA_MSG_SEND_DIRECT_REQ_SMC32 &&
		    ffa_func_id(ffa_ret) != FFA_MSG_SEND_DIRECT_REQ_SMC64) {
			ERROR("%s(%u) unknown func id 0x%x\n",
				__func__, vm_id, ffa_func_id(ffa_ret));
			break;
		}

		destination = ffa_dir_msg_dest(ffa_ret);

		source = ffa_dir_msg_source(ffa_ret);

		if (destination != vm_id) {
			ERROR("%s(%u) invalid vm id 0x%x\n",
				__func__, vm_id, destination);
			break;
		}

		PRINT_CMD(ffa_ret);

		cactus_cmd = cactus_get_cmd(ffa_ret);

		switch (cactus_cmd) {
		case CACTUS_MEM_SEND_CMD:
			ffa_memory_management_test(
					mb, vm_id, source,
					cactus_req_mem_send_get_mem_func(
						ffa_ret),
					cactus_mem_send_get_handle(ffa_ret));

			/*
			 * If execution gets to this point means all operations
			 * with memory retrieval went well, as such replying
			 */
			ffa_ret = cactus_success_resp(vm_id, source, 0);
			break;
		case CACTUS_REQ_MEM_SEND_CMD:
		{
			uint32_t mem_func =
				cactus_req_mem_send_get_mem_func(ffa_ret);
			ffa_vm_id_t receiver =
				cactus_req_mem_send_get_receiver(ffa_ret);
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

			ffa_ret = cactus_mem_send_cmd(vm_id, receiver, mem_func,
						      handle);

			if (!is_ffa_direct_response(ffa_ret)) {
				ERROR("Failed to send message. error: %x\n",
					ffa_error_code(ffa_ret));
				ffa_ret = cactus_error_resp(vm_id, source, 0);
				break;
			}

			/* If anything went bad on the receiver's end. */
			if (cactus_get_response(ffa_ret) == CACTUS_ERROR) {
				ERROR("Received error from receiver!\n");
				ffa_ret = cactus_error_resp(vm_id, source, 0);
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
					ffa_ret = cactus_error_resp(vm_id,
								    source, 0);
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

			ffa_ret = cactus_success_resp(vm_id, source, 0);
			break;
		}
		case CACTUS_ECHO_CMD:
		{
			uint64_t echo_val = cactus_echo_get_val(ffa_ret);

			VERBOSE("Received echo at %x, value %llx from %x.\n",
				destination, echo_val, source);
			ffa_ret = cactus_response(destination, source, echo_val);
			break;
		}
		case CACTUS_REQ_ECHO_CMD:
		{
			ffa_vm_id_t echo_dest =
					cactus_req_echo_get_echo_dest(ffa_ret);
			uint64_t echo_val = cactus_echo_get_val(ffa_ret);
			bool success = true;

			VERBOSE("%x requested to send echo to %x, value %llx\n",
				source, echo_dest, echo_val);

			ffa_ret = cactus_echo_send_cmd(vm_id, echo_dest,
							echo_val);

			if (!is_ffa_direct_response(ffa_ret)) {
				ERROR("Failed to send message. error: %x\n",
					ffa_error_code(ffa_ret));
				success = false;
			}

			if (cactus_get_response(ffa_ret) != echo_val) {
				ERROR("Echo Failed!\n");
				success = false;
			}

			ffa_ret = success ?
				  cactus_success_resp(vm_id, source, 0) :
				  cactus_error_resp(vm_id, source, 0);
			break;
		}
		case CACTUS_DEADLOCK_CMD:
		case CACTUS_REQ_DEADLOCK_CMD:
		{
			ffa_vm_id_t deadlock_dest =
				cactus_deadlock_get_next_dest(ffa_ret);
			ffa_vm_id_t deadlock_next_dest = source;

			if (cactus_cmd == CACTUS_DEADLOCK_CMD) {
				VERBOSE("%x is creating deadlock. next: %x\n",
					source, deadlock_dest);
			} else if (cactus_cmd == CACTUS_REQ_DEADLOCK_CMD) {
				VERBOSE(
				"%x requested deadlock with %x and %x\n",
				source, deadlock_dest, deadlock_next_dest);

				deadlock_next_dest =
					cactus_deadlock_get_next_dest2(ffa_ret);
			}

			ffa_ret = cactus_deadlock_send_cmd(vm_id, deadlock_dest,
							   deadlock_next_dest);

			/*
			 * Should be true for the last partition to attempt
			 * an FF-A direct message, to the first partition.
			 */
			bool is_deadlock_detected =
				(ffa_func_id(ffa_ret) == FFA_ERROR) &&
				(ffa_error_code(ffa_ret) == FFA_ERROR_BUSY);

			/*
			 * Should be true after the deadlock has been detected
			 * and after the first response has been sent down the
			 * request chain.
			 */
			bool is_returning_from_deadlock =
				(is_ffa_direct_response(ffa_ret))
				&&
				(cactus_get_response(ffa_ret) == CACTUS_SUCCESS);

			if (is_deadlock_detected) {
				NOTICE("Attempting dealock but got error %x\n",
					ffa_error_code(ffa_ret));
			}

			if (is_deadlock_detected ||
			    is_returning_from_deadlock) {
				/*
				 * This is not the partition, that would have
				 * created the deadlock. As such, reply back
				 * to the partitions.
				 */
				ffa_ret = cactus_success_resp(vm_id, source, 0);
				break;
			}

			/* Shouldn't get to this point */
			ERROR("Deadlock test went wrong!\n");
			ffa_ret = cactus_error_resp(vm_id, source, 0);
			break;
		}
		case CACTUS_REQ_SIMD_FILL_CMD:
			fill_simd_vectors();
			ffa_ret = cactus_success_resp(vm_id, source, 0);
			break;
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
	if (ffa_func_id(ffa_id_ret) != FFA_SUCCESS_SMC32) {
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
			if (ffa_func_id(ret) != FFA_SUCCESS_SMC32) {
				ERROR(
				    "Failed to map RXTX buffers. Error: %x\n",
				    ffa_error_code(ret));
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
