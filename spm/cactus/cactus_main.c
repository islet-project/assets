/*
 * Copyright (c) 2018-2020, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>
#include <cactus_def.h>
#include <debug.h>
#include <drivers/arm/pl011.h>
#include <drivers/console.h>
#include <errno.h>
#include <lib/aarch64/arch_helpers.h>
#include <lib/xlat_tables/xlat_tables_v2.h>
#include <lib/xlat_tables/xlat_mmu_helpers.h>
#include <plat_arm.h>
#include <plat/common/platform.h>
#include <platform_def.h>
#include <sp_helpers.h>
#include <spci_svc.h>
#include <std_svc.h>

#include "cactus.h"
#include "cactus_def.h"
#include "cactus_tests.h"

#include "tftf_lib.h"

#define SPM_VM_ID_FIRST			(1)

#define SPM_VM_GET_COUNT		(0xFF01)
#define SPM_VCPU_GET_COUNT		(0xFF02)
#define SPM_DEBUG_LOG			(0xBD000000)

/* Hypervisor ID at physical SPCI instance */
#define HYP_ID		(0)

/* By convention, SP IDs (as opposed to VM IDs) have bit 15 set */
#define SP_ID(x)	(x | (1 << 15))

typedef unsigned short spci_vm_id_t;
typedef unsigned short spci_vm_count_t;
typedef unsigned short spci_vcpu_count_t;

/* Host machine information injected by the build system in the ELF file. */
extern const char build_message[];
extern const char version_string[];

static spci_vcpu_count_t spm_vcpu_get_count(spci_vm_id_t vm_id)
{
	hvc_args args = {
		.fid = SPM_VCPU_GET_COUNT,
		.arg1 = vm_id
	};

	hvc_ret_values ret = tftf_hvc(&args);

	return ret.ret0;
}

static spci_vm_count_t spm_vm_get_count(void)
{
	hvc_args args = {
		.fid = SPM_VM_GET_COUNT
	};

	hvc_ret_values ret = tftf_hvc(&args);

	return ret.ret0;
}

static void spm_debug_log(char c)
{
	hvc_args args = {
		.fid = SPM_DEBUG_LOG,
		.arg1 = c
	};

	(void)tftf_hvc(&args);
}

static smc_ret_values spci_id_get(void)
{
	smc_args args = {
		.fid = SPCI_ID_GET
	};

	return tftf_smc(&args);
}

static smc_ret_values spci_msg_wait(void)
{
	smc_args args = {
		.fid = SPCI_MSG_WAIT
	};

	return tftf_smc(&args);
}

/* Send response through registers using direct messaging */
static smc_ret_values spci_msg_send_direct_resp(spci_vm_id_t sender_vm_id,
					 spci_vm_id_t target_vm_id,
					 uint32_t message)
{
	smc_args args = {
		.fid = SPCI_MSG_SEND_DIRECT_RESP_SMC32,
		.arg1 = ((uint32_t)sender_vm_id << 16) | target_vm_id,
		.arg3 = message
	};

	return tftf_smc(&args);
}

static smc_ret_values spci_error(int32_t error_code)
{
	smc_args args = {
		.fid = SPCI_ERROR,
		.arg1 = 0,
		.arg2 = error_code
	};

	return tftf_smc(&args);
}

/*
 *
 * Message loop function
 * Notice we cannot use regular print functions because this serves to both
 * "primary" and "secondary" VMs. Secondary VM cannot access UART directly
 * but rather through Hafnium print hypercall.
 *
 */
static void __dead2 message_loop(spci_vm_id_t vm_id)
{
	smc_ret_values spci_ret;
	uint32_t sp_response;

	/*
	 * This initial wait call is necessary to inform SPMD that
	 * SP initialization has completed. It blocks until receiving
	 * a direct message request.
	 */
	spci_ret = spci_msg_wait();

	for (;;) {

		if (spci_ret.ret0 != SPCI_MSG_SEND_DIRECT_REQ_SMC32) {
			spci_ret = spci_error(-1);
			continue;
		}

		if (spci_ret.ret1 != SP_ID(vm_id)) {
			spci_ret = spci_error(-2);
			continue;
		}

		if (spci_ret.ret2 != HYP_ID) {
			spci_ret = spci_error(-3);
			continue;
		}

		/*
		 * For the sake of testing, add the vm id to the
		 * received message.
		 */
		sp_response = spci_ret.ret3 | vm_id;

		/*
		 * Send a response through direct messaging then block
		 * until receiving a new message request.
		 */
		spci_ret = spci_msg_send_direct_resp(SP_ID(vm_id),
						     HYP_ID, sp_response);
	}
}

static const mmap_region_t cactus_mmap[] __attribute__((used)) = {
	/* DEVICE0 area includes UART2 necessary to console */
	MAP_REGION_FLAT(DEVICE0_BASE, DEVICE0_SIZE, MT_DEVICE | MT_RW),
	{0}
};

static void cactus_print_memory_layout(void)
{
	NOTICE("Secure Partition memory layout:\n");

	NOTICE("  Image regions\n");
	NOTICE("    Text region            : %p - %p\n",
		(void *)CACTUS_TEXT_START, (void *)CACTUS_TEXT_END);
	NOTICE("    Read-only data region  : %p - %p\n",
		(void *)CACTUS_RODATA_START, (void *)CACTUS_RODATA_END);
	NOTICE("    Data region            : %p - %p\n",
		(void *)CACTUS_DATA_START, (void *)CACTUS_DATA_END);
	NOTICE("    BSS region             : %p - %p\n",
		(void *)CACTUS_BSS_START, (void *)CACTUS_BSS_END);
	NOTICE("    Total image memory     : %p - %p\n",
		(void *)CACTUS_IMAGE_BASE,
		(void *)(CACTUS_IMAGE_BASE + CACTUS_IMAGE_SIZE));
	NOTICE("  SPM regions\n");
	NOTICE("    SPM <-> SP buffer      : %p - %p\n",
		(void *)CACTUS_SPM_BUF_BASE,
		(void *)(CACTUS_SPM_BUF_BASE + CACTUS_SPM_BUF_SIZE));
	NOTICE("    NS <-> SP buffer       : %p - %p\n",
		(void *)CACTUS_NS_BUF_BASE,
		(void *)(CACTUS_NS_BUF_BASE + CACTUS_NS_BUF_SIZE));
	NOTICE("  Test regions\n");
	NOTICE("    Test region            : %p - %p\n",
		(void *)CACTUS_TEST_MEM_BASE,
		(void *)(CACTUS_TEST_MEM_BASE + CACTUS_TEST_MEM_SIZE));
}

static void cactus_plat_configure_mmu(void)
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

	mmap_add(cactus_mmap);
	init_xlat_tables();
}

void __dead2 cactus_main(void)
{
	assert(IS_IN_EL1() != 0);

	/* Clear BSS */
	memset((void *)CACTUS_BSS_START,
	       0, CACTUS_BSS_END - CACTUS_BSS_START);

	/* Configure and enable Stage-1 MMU, enable D-Cache */
	cactus_plat_configure_mmu();
	enable_mmu_el1(0);

	/* Get current SPCI id */
	smc_ret_values spci_id_ret = spci_id_get();
	if (spci_id_ret.ret0 != SPCI_SUCCESS_SMC32) {
		ERROR("SPCI_ID_GET failed.\n");
		panic();
	}

	spci_vm_id_t spci_id = spci_id_ret.ret2 & 0xffff;
	if (spci_id > SPM_VM_ID_FIRST) {
		/* Indicate secondary VM start through debug log hypercall */
		spm_debug_log('2');
		spm_debug_log('N');
		spm_debug_log('D');
		spm_debug_log('\n');

		/* Run straight to the message loop */
		message_loop(spci_id);
	}

	/* Next initialization steps only performed by primary VM */

	console_init(PL011_UART2_BASE,
		     PL011_UART2_CLK_IN_HZ,
		     PL011_BAUDRATE);

	NOTICE("Booting Cactus Secure Partition\n%s\n%s\n",
	       build_message, version_string);

	cactus_print_memory_layout();

	NOTICE("SPCI id: %u\n", spci_id); /* Expect VM id 1 */

	/* Get number of VMs */
	NOTICE("VM count: %u\n", spm_vm_get_count());

	/* Get virtual CPU count for current VM */
	NOTICE("vCPU count: %u\n", spm_vcpu_get_count(spci_id));

	/* End up to message loop */
	message_loop(spci_id);

	/* Not reached */
}
