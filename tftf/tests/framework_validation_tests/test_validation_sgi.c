/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <arch_helpers.h>
#include <debug.h>
#include <drivers/arm/arm_gic.h>
#include <irq.h>
#include <platform.h>
#include <sgi.h>
#include <tftf_lib.h>

/*
 * The 2 following global variables are used by the SGI handler to return
 * information to the main test function.
 */
static sgi_data_t sgi_data;

/* Flag to indicate whether the SGI has been handled */
static volatile unsigned int sgi_handled;

static int sgi_handler(void *data)
{
	/* Save SGI data */
	sgi_data = *(sgi_data_t *) data;
	sgi_handled = 1;

	/* Return value doesn't matter */
	return 0;
}

/*
 * @Test_Aim@ Test SGI support on lead CPU
 *
 * 1) Register a local IRQ handler for SGI 0.
 * 2) Send SGI 0 to the calling core, i.e. the lead CPU.
 * 3) Check the correctness of the data received in the IRQ handler.
 *
 * TODO: Improve this test by sending SGIs to all cores in the system.
 *       This will ensure that IRQs are correctly configured on all cores.
 */
test_result_t test_validation_sgi(void)
{
	unsigned int mpid = read_mpidr_el1();
	unsigned int core_pos = platform_get_core_pos(mpid);
	const unsigned int sgi_id = IRQ_NS_SGI_0;
	test_result_t test_res = TEST_RESULT_SUCCESS;
	int ret;

	/* Register the local IRQ handler for the SGI */
	ret = tftf_irq_register_handler(sgi_id, sgi_handler);
	if (ret != 0) {
		tftf_testcase_printf("Failed to register IRQ %u (%d)",
				sgi_id, ret);
		return TEST_RESULT_FAIL;
	}
	tftf_irq_enable(sgi_id, GIC_HIGHEST_NS_PRIORITY);

	/* Send the SGI to the lead CPU */
	tftf_send_sgi(sgi_id, core_pos);

	/*
	 * Wait for the SGI to be handled.
	 * The SGI handler will update a global variable to reflect that.
	 */
	while (sgi_handled == 0)
		continue;

	/* Verify the data received in the SGI handler */
	if (sgi_data.irq_id != sgi_id) {
		tftf_testcase_printf("Wrong IRQ ID, expected %u, got %u\n",
			sgi_id, sgi_data.irq_id);
		test_res = TEST_RESULT_FAIL;
	}

	tftf_irq_disable(sgi_id);
	tftf_irq_unregister_handler(sgi_id);

	return test_res;
}
