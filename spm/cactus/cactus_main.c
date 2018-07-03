/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>
#include <console.h>
#include <debug.h>
#include <pl011.h>
#include <plat_arm.h>
#include <platform_def.h>
#include <secure_partition.h>
#include <sp_helpers.h>
#include <spm_svc.h>
#include <std_svc.h>

#include "cactus.h"
#include "cactus_tests.h"


/* Host machine information injected by the build system in the ELF file. */
extern const char build_message[];
extern const char version_string[];

/*
 * The ARM Trusted Firmware passes a description of the memory resources
 * allocated to the secure partition through the x0 register. This maps to
 * a secure_partition_boot_info_t structure type.
 *
 * This functions prints the information stored in this structure.
 */
static void cactus_print_memory_layout(const secure_partition_boot_info_t *boot_info)
{
	NOTICE("Secure Partition memory layout:\n");

	NOTICE("  Secure Partition image   : %p - %p\n",
		(void *) boot_info->sp_image_base,
		(void *)(boot_info->sp_image_base + boot_info->sp_image_size));
	NOTICE("    Text region            : %p - %p\n",
		(void *) CACTUS_TEXT_START, (void *) CACTUS_TEXT_END);
	NOTICE("    Read-only data region  : %p - %p\n",
		(void *) CACTUS_RODATA_START, (void *) CACTUS_RODATA_END);
	NOTICE("    Read-write data region : %p - %p\n",
		(void *) CACTUS_RWDATA_START, (void *) CACTUS_RWDATA_END);
	NOTICE("      BSS region           : %p - %p\n",
		(void *) CACTUS_BSS_START, (void *) CACTUS_BSS_END);
	NOTICE("    Unused SP image space  : %p - %p\n",
		(void *) CACTUS_BSS_END,
		(void *)(boot_info->sp_image_base + boot_info->sp_image_size));

	NOTICE("  EL3-EL0 shared buffer    : %p - %p\n",
		(void *) boot_info->sp_shared_buf_base,
		(void *)(boot_info->sp_shared_buf_base + boot_info->sp_shared_buf_size));

	NOTICE("  S-NS shared buffer       : %p - %p\n",
		(void *) boot_info->sp_ns_comm_buf_base,
		(void *)(boot_info->sp_ns_comm_buf_base + boot_info->sp_ns_comm_buf_size));

	assert(boot_info->sp_ns_comm_buf_base == ARM_SECURE_SERVICE_BUFFER_BASE);
	assert(boot_info->sp_ns_comm_buf_size == ARM_SECURE_SERVICE_BUFFER_SIZE);

	NOTICE("  Stacks region (%u CPUS)   : %p - %p\n",
		boot_info->num_cpus,
		(void *) boot_info->sp_stack_base,
		(void *)(boot_info->sp_stack_base +
			 (boot_info->sp_pcpu_stack_size * boot_info->num_cpus)));

	NOTICE("  Heap region              : %p - %p\n",
		(void *) boot_info->sp_heap_base,
		(void *)(boot_info->sp_heap_base + boot_info->sp_heap_size));

	NOTICE("Total memory               : %p - %p\n",
		(void *) boot_info->sp_mem_base, (void *) boot_info->sp_mem_limit);
}


void __dead2 cactus_main(void *el3_el0_buffer, size_t el3_el0_buffer_size)
{
	console_init(PL011_UART2_BASE,
		     PL011_UART2_CLK_IN_HZ,
		     PL011_BAUDRATE);

	NOTICE("Booting test Secure Partition Cactus\n");
	NOTICE("%s\n", build_message);
	NOTICE("%s\n", version_string);
	NOTICE("Running at S-EL0\n");

	const secure_partition_boot_info_t *boot_info =
		(const secure_partition_boot_info_t *) el3_el0_buffer;

	if (el3_el0_buffer_size < sizeof(secure_partition_boot_info_t)) {
		ERROR("The memory buffer shared between EL3/S-EL0 is too small\n");
		ERROR("It is %lu bytes, it should be at least %lu bytes\n",
			el3_el0_buffer_size,
			sizeof(secure_partition_boot_info_t));
		panic();
	}

	if ((CACTUS_TEXT_START != boot_info->sp_image_base) ||
	    (CACTUS_RWDATA_END > boot_info->sp_image_base
		    + boot_info->sp_image_size)) {
		ERROR("Cactus does not fit in the buffer allocated for the secure partition\n");
		panic();
	}

	cactus_print_memory_layout(boot_info);


	/*
	 * Run some initial tests.
	 *
	 * These are executed when the system is still booting, just after SPM
	 * has handed over to Cactus.
	 */
	misc_tests();
	system_setup_tests();
	mem_attr_changes_tests(boot_info);

	/*
	 * Handle secure service requests.
	 */
	secure_services_loop();
}
