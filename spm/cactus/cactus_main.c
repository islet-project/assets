/*
 * Copyright (c) 2018-2020, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>
#include <errno.h>

#include "cactus.h"
#include "cactus_def.h"
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

/* Host machine information injected by the build system in the ELF file. */
extern const char build_message[];
extern const char version_string[];

/*
 *
 * Message loop function
 * Notice we cannot use regular print functions because this serves to both
 * "primary" and "secondary" VMs. Secondary VM cannot access UART directly
 * but rather through Hafnium print hypercall.
 *
 */
static void __dead2 message_loop(ffa_vm_id_t vm_id)
{
	smc_ret_values ffa_ret;
	uint32_t sp_response;

	/*
	 * This initial wait call is necessary to inform SPMD that
	 * SP initialization has completed. It blocks until receiving
	 * a direct message request.
	 */
	ffa_ret = ffa_msg_wait();

	for (;;) {

		if (ffa_ret.ret0 != FFA_MSG_SEND_DIRECT_REQ_SMC32) {
			ffa_ret = ffa_error(-1);
			continue;
		}

		if (ffa_ret.ret1 != SP_ID(vm_id)) {
			ffa_ret = ffa_error(-2);
			continue;
		}

		if (ffa_ret.ret2 != HYP_ID) {
			ffa_ret = ffa_error(-3);
			continue;
		}

		/*
		 * For the sake of testing, add the vm id to the
		 * received message.
		 */
		sp_response = ffa_ret.ret3 | vm_id;

		/*
		 * Send a response through direct messaging then block
		 * until receiving a new message request.
		 */
		ffa_ret = ffa_msg_send_direct_resp(SP_ID(vm_id),
						     HYP_ID, sp_response);
	}
}

static const mmap_region_t cactus_mmap[] __attribute__((used)) = {
	/* DEVICE0 area includes UART2 necessary to console */
	MAP_REGION_FLAT(DEVICE0_BASE, DEVICE0_SIZE, MT_DEVICE | MT_RW),
	{0}
};

static void cactus_print_memory_layout(unsigned int vm_id)
{
	NOTICE("Secure Partition memory layout:\n");

	NOTICE("  Text region            : %p - %p\n",
		(void *)CACTUS_TEXT_START, (void *)CACTUS_TEXT_END);

	NOTICE("  Read-only data region  : %p - %p\n",
		(void *)CACTUS_RODATA_START, (void *)CACTUS_RODATA_END);

	NOTICE("  Data region            : %p - %p\n",
		(void *)CACTUS_DATA_START, (void *)CACTUS_DATA_END);

	NOTICE("  BSS region             : %p - %p\n",
		(void *)CACTUS_BSS_START, (void *)CACTUS_BSS_END);

	NOTICE("  RX                     : %p - %p\n",
		(void *)get_sp_rx_start(vm_id),
		(void *)get_sp_rx_end(vm_id));

	NOTICE("  TX                     : %p - %p\n",
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
		console_init(PL011_UART2_BASE,
			PL011_UART2_CLK_IN_HZ,
			PL011_BAUDRATE);

		set_putc_impl(PL011_AS_STDOUT);

		NOTICE("Booting Primary Cactus Secure Partition\n%s\n%s\n",
			build_message, version_string);

		/* Get number of VMs */
		NOTICE("VM count: %u\n", spm_vm_get_count());

		/* Get virtual CPU count for current VM */
		NOTICE("vCPU count: %u\n", spm_vcpu_get_count(ffa_id));
	} else {
		set_putc_impl(HVC_CALL_AS_STDOUT);

		NOTICE("Booting Secondary Cactus Secure Partition\n%s\n%s\n",
			build_message, version_string);
	}

	NOTICE("FFA id: %u\n", ffa_id);
	cactus_print_memory_layout(ffa_id);

	/* Invoking Tests */
	ffa_tests(&mb);

	/* End up to message loop */
	message_loop(ffa_id);

	/* Not reached */
}
