/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <arch_helpers.h>
#include <arm_gic.h>
#include <assert.h>
#include <debug.h>
#include <irq.h>
#include <mmio.h>
#include <platform.h>
#include <platform_def.h>
#include <sgi.h>
#include <smccc.h>
#include <std_svc.h>
#include <stdlib.h>
#include <string.h>
#include <test_helpers.h>
#include <tftf.h>

#define STRESS_COUNT 100

/*
 * The shared data between the handler and the
 * preempt_tsp_via_SGI routine.
 */
typedef struct {
	smc_ret_values tsp_result;
	int wait_for_fiq;
} irq_handler_shared_data;

static irq_handler_shared_data shared_data;

/*
 * Handler for the SGI #0.
 */
static int sgi_handler(void *data)
{
	/* Ensure this is the SGI we expect */
	assert(*(unsigned int *)data == IRQ_NS_SGI_0);

	if (shared_data.wait_for_fiq)
		wfi(); /* We will get woken by the FIQ firing */

	return 0;
}

/*
 * This routine issues a SGI with interrupts disabled to make sure that the
 * pending SGI will preempt a STD SMC.
 */
static test_result_t preempt_tsp_via_SGI(const smc_args *tsp_svc_params,
				int hold_irq_handler_for_fiq)
{
	int rc;
	unsigned int core_mpid = read_mpidr_el1() & MPID_MASK;
	unsigned int core_pos = platform_get_core_pos(core_mpid);
	test_result_t result = TEST_RESULT_SUCCESS;

	memset(&shared_data, 0, sizeof(shared_data));

	if (hold_irq_handler_for_fiq)
		shared_data.wait_for_fiq = 1;

	/* Register Handler for the interrupt. SGIs #0 - #6 are available. */
	rc = tftf_irq_register_handler(IRQ_NS_SGI_0, sgi_handler);
	if (rc != 0) {
		tftf_testcase_printf("Failed to register SGI handler. "
				"Error code = %d\n", rc);
		return TEST_RESULT_SKIPPED;
	}

	/* Enable SGI #0 */
	tftf_irq_enable(IRQ_NS_SGI_0, GIC_HIGHEST_NS_PRIORITY);

	/* Set PSTATE.I to 0. */
	disable_irq();

	/*
	 * Send SGI to the current CPU. It can't be handled because the
	 * interrupts are disabled.
	 */
	tftf_send_sgi(IRQ_NS_SGI_0, core_pos);

	/*
	 * Invoke an STD SMC. Should be pre-empted because of the SGI that is
	 * waiting.
	 */
	shared_data.tsp_result = tftf_smc(tsp_svc_params);
	if (shared_data.tsp_result.ret0 != TSP_SMC_PREEMPTED) {
		tftf_testcase_printf("SMC returned 0x%llX instead of "
				     "TSP_SMC_PREEMPTED.\n",
				     (unsigned long long)shared_data.tsp_result.ret0);
		result = TEST_RESULT_FAIL;
	}

	/* Set PSTATE.I to 1. The SGI will be handled after this. */
	enable_irq();

	/* Disable SGI #0 */
	tftf_irq_disable(IRQ_NS_SGI_0);

	/* Unregister handler */
	rc = tftf_irq_unregister_handler(IRQ_NS_SGI_0);
	if (rc != 0) {
		tftf_testcase_printf("Failed to unregister IRQ handler. "
				     "Error code = %d\n", rc);
		result = TEST_RESULT_FAIL;
	}

	return result;
}

/*
 * @Test_Aim@ Test the secure world preemption by non secure interrupt.
 *
 * Steps: 1. Issue Standard SMC and preempt it via SGI
 *        2. Resume the preempted SMC
 * Returns SUCCESS if above 2 steps are performed correctly else failure.
 */
test_result_t tsp_int_and_resume(void)
{
	smc_args tsp_svc_params;
	smc_ret_values tsp_result = {0};
	test_result_t res;

	SKIP_TEST_IF_TSP_NOT_PRESENT();

	/* Standard SMC */
	tsp_svc_params.fid = TSP_STD_FID(TSP_ADD);
	tsp_svc_params.arg1 = 4;
	tsp_svc_params.arg2 = 6;
	res = preempt_tsp_via_SGI(&tsp_svc_params, 0);
	if (res == TEST_RESULT_FAIL)
		return res;

	/* Now that we have ensured preemption, issue RESUME */
	tsp_svc_params.fid = TSP_FID_RESUME;
	tsp_result = tftf_smc(&tsp_svc_params);

	/* Check the result of the addition */
	if (tsp_result.ret0 != 0 || tsp_result.ret1 != 8 ||
					tsp_result.ret2 != 12) {
		tftf_testcase_printf("SMC resume returned wrong result:"
				     "got %d %d %d expected: 0 8 12\n",
						(unsigned int)tsp_result.ret0,
						(unsigned int)tsp_result.ret1,
						(unsigned int)tsp_result.ret2);
		tftf_testcase_printf("SMC resume returned wrong result\n");
		return TEST_RESULT_FAIL;
	}

	/* Standard SMC */
	tsp_svc_params.fid = TSP_STD_FID(TSP_SUB);
	tsp_svc_params.arg1 = 4;
	tsp_svc_params.arg2 = 6;
	res = preempt_tsp_via_SGI(&tsp_svc_params, 0);
	if (res == TEST_RESULT_FAIL)
		return res;

	/* Now that we have ensured preemption, issue RESUME */
	tsp_svc_params.fid = TSP_FID_RESUME;
	tsp_result = tftf_smc(&tsp_svc_params);

	/* Check the result of the substraction */
	if (tsp_result.ret0 != 0 || tsp_result.ret1 != 0 ||
						tsp_result.ret2 != 0) {
		tftf_testcase_printf("SMC resume returned wrong result:"
				     "got %d %d %d expected: 0 0 0\n",
						(unsigned int)tsp_result.ret0,
						(unsigned int)tsp_result.ret1,
						(unsigned int)tsp_result.ret2);
		return TEST_RESULT_FAIL;
	}

	/* Standard SMC */
	tsp_svc_params.fid = TSP_STD_FID(TSP_MUL);
	tsp_svc_params.arg1 = 4;
	tsp_svc_params.arg2 = 6;
	res = preempt_tsp_via_SGI(&tsp_svc_params, 0);
	if (res == TEST_RESULT_FAIL)
		return res;

	/* Now that we have ensured preemption, issue RESUME */
	tsp_svc_params.fid = TSP_FID_RESUME;
	tsp_result = tftf_smc(&tsp_svc_params);

	/* Check the result of the multiplication */
	if (tsp_result.ret0 != 0 || tsp_result.ret1 != 16 ||
						tsp_result.ret2 != 36) {
		tftf_testcase_printf("SMC resume returned wrong result:"
				     "got %d %d %d expected: 0 16 36\n",
						(unsigned int)tsp_result.ret0,
						(unsigned int)tsp_result.ret1,
						(unsigned int)tsp_result.ret2);
		return TEST_RESULT_FAIL;
	}

	/* Standard SMC */
	tsp_svc_params.fid = TSP_STD_FID(TSP_DIV);
	tsp_svc_params.arg1 = 4;
	tsp_svc_params.arg2 = 6;
	res = preempt_tsp_via_SGI(&tsp_svc_params, 0);
	if (res == TEST_RESULT_FAIL)
		return res;

	/* Now that we have ensured preemption, issue RESUME */
	tsp_svc_params.fid = TSP_FID_RESUME;
	tsp_result = tftf_smc(&tsp_svc_params);

	/* Check the result of the division */
	if (tsp_result.ret0 != 0 || tsp_result.ret1 != 1 ||
					tsp_result.ret2 != 1) {
		tftf_testcase_printf("SMC resume returned wrong result:"
				     "got %d %d %d expected: 0 1 1\n",
						(unsigned int)tsp_result.ret0,
						(unsigned int)tsp_result.ret1,
						(unsigned int)tsp_result.ret2);
		return TEST_RESULT_FAIL;
	}

	return TEST_RESULT_SUCCESS;
}

/*
 * @Test_Aim@ Verify Fast SMC request on an interrupted tsp returns error.
 *
 * Steps: 1. Issue Standard SMC and preempt it via SGI
 * 2. Issue Fast SMC, this is not expected and TSP should return error.
 * 3. Resume the preempted SMC and verify the result.
 * Returns SUCCESS if above 3 steps are performed correctly else failure.
 */
test_result_t test_fast_smc_when_tsp_preempted(void)
{
	smc_args tsp_svc_params;
	smc_ret_values tsp_result = {0};
	test_result_t res = TEST_RESULT_SUCCESS;

	SKIP_TEST_IF_TSP_NOT_PRESENT();

	/* Standard SMC */
	tsp_svc_params.fid = TSP_STD_FID(TSP_ADD);
	tsp_svc_params.arg1 = 4;
	tsp_svc_params.arg2 = 6;
	res = preempt_tsp_via_SGI(&tsp_svc_params, 0);
	if (res == TEST_RESULT_FAIL)
		return res;

	/* Now that we have ensured preemption, issue Fast SMC */
	tsp_svc_params.fid = TSP_FAST_FID(TSP_ADD);
	tsp_svc_params.arg1 = 4;
	tsp_svc_params.arg2 = 6;

	tsp_result = tftf_smc(&tsp_svc_params);

	if (tsp_result.ret0 != SMC_UNKNOWN) {
		tftf_testcase_printf("Fast SMC should not execute"
				"while SMC is preempted\n");
		res = TEST_RESULT_FAIL;
	}

	/* Issue RESUME */
	tsp_svc_params.fid = TSP_FID_RESUME;
	tsp_result = tftf_smc(&tsp_svc_params);

	/* Check the result of the addition */
	if (tsp_result.ret0 != 0 || tsp_result.ret1 != 8 ||
				tsp_result.ret2 != 12) {
		tftf_testcase_printf("SMC resume returned wrong result:"
				     "got %d %d %d expected: 0 8 12\n",
						(unsigned int)tsp_result.ret0,
						(unsigned int)tsp_result.ret1,
						(unsigned int)tsp_result.ret2);

		res = TEST_RESULT_FAIL;
	}

	return res;
}

/*
 * @Test_Aim@ Test the Standard SMC when tsp is pre-empted by interrupt.
 *
 * Steps:
 * 1. Issue Standard SMC and preempt it via SGI
 * 2. Issue another Standard SMC. this is not expected and TSP should return
 *    error.
 * 3. Resume the preempted SMC or abort if the parameter `abort_smc` is set to
 *    1.
 * 4. Check the result if the SMC was resumed, or just carry on if it was
 *    aborted.
 * Returns SUCCESS if above 4 steps are performed correctly else failure.
 */
static test_result_t test_std_smc_when_tsp_preempted(int abort_smc)
{
	smc_args tsp_svc_params;
	smc_ret_values tsp_result = {0};
	test_result_t res = TEST_RESULT_SUCCESS;

	SKIP_TEST_IF_TSP_NOT_PRESENT();

	/* Standard SMC */
	tsp_svc_params.fid = TSP_STD_FID(TSP_ADD);
	tsp_svc_params.arg1 = 4;
	tsp_svc_params.arg2 = 6;
	res = preempt_tsp_via_SGI(&tsp_svc_params, 0);
	if (res == TEST_RESULT_FAIL)
		return res;

	/* Now that we have ensured preemption, issue Standard SMC */
	tsp_svc_params.fid = TSP_STD_FID(TSP_ADD);
	tsp_svc_params.arg1 = 4;
	tsp_svc_params.arg2 = 6;

	tsp_result = tftf_smc(&tsp_svc_params);

	if (tsp_result.ret0 != SMC_UNKNOWN) {
		tftf_testcase_printf("Standard SMC should not execute while SMC is preempted\n");
		res = TEST_RESULT_FAIL;
	}

	/* Issue ABORT or RESUME */
	tsp_svc_params.fid = abort_smc ? TSP_FID_ABORT : TSP_FID_RESUME;
	tsp_result = tftf_smc(&tsp_svc_params);

	/*
	 * There is no way to check that the ABORT succeeded or failed because
	 * it will return SMC_UNKNOWN in both cases.
	 */
	if (!abort_smc) {
		/*
		 * Check the result of the addition if we issued RESUME.
		 */
		if (tsp_result.ret0 != 0 || tsp_result.ret1 != 8 ||
		     tsp_result.ret2 != 12) {
			tftf_testcase_printf("SMC resume returned wrong result: got %d %d %d expected: 0 8 12\n",
					     (unsigned int)tsp_result.ret0,
					     (unsigned int)tsp_result.ret1,
					     (unsigned int)tsp_result.ret2);
			res = TEST_RESULT_FAIL;
		}
	}

	return res;
}

test_result_t test_std_smc_when_tsp_preempted_resume(void)
{
	return test_std_smc_when_tsp_preempted(0);
}

test_result_t test_std_smc_when_tsp_preempted_abort(void)
{
	return test_std_smc_when_tsp_preempted(1);
}

/*
 * @Test_Aim@ Test RESUME SMC call when TSP is not preempted. RESUME should fail.
 *
 * Issues resume SMC. This is not expected by TSP and returns error.
 * This is a negative test, Return SUCCESS is RESUME returns SMC_UNKNOWN
 */
test_result_t test_resume_smc_without_preemption(void)
{
	smc_args tsp_svc_params;
	smc_ret_values tsp_result = {0};

	SKIP_TEST_IF_TSP_NOT_PRESENT();

	/* Issue RESUME */
	tsp_svc_params.fid = TSP_FID_RESUME;
	tsp_result = tftf_smc(&tsp_svc_params);

	if (tsp_result.ret0 != SMC_UNKNOWN) {
		tftf_testcase_printf("SMC Resume should return UNKNOWN, got:%d\n", \
							(unsigned int)tsp_result.ret0);
		return TEST_RESULT_FAIL;
	}

	return TEST_RESULT_SUCCESS;
}

/*
 * @Test_Aim@ Stress Test the secure world preemption by non secure interrupt
 *
 * Steps: 1. Issue Standard SMC and preempt it via SGI
 * 2. Resume the preempted SMC and repeat from Step 1 for STRESS_COUNT times.
 * Returns SUCCESS if above 2 steps are performed correctly else failure.
 */
test_result_t tsp_int_and_resume_stress(void)
{
	smc_args tsp_svc_params;
	smc_ret_values tsp_result = {0};
	test_result_t res = TEST_RESULT_SUCCESS;

	SKIP_TEST_IF_TSP_NOT_PRESENT();

	int count = 0;

	NOTICE("This stress test will repeat %d times\n", STRESS_COUNT);
	while ((count < STRESS_COUNT) &&
			(res == TEST_RESULT_SUCCESS)) {
		/* Standard SMC */
		tsp_svc_params.fid = TSP_STD_FID(TSP_ADD);
		tsp_svc_params.arg1 = 4;
		tsp_svc_params.arg2 = 6;
		/* Try to preempt TSP via IRQ */
		res = preempt_tsp_via_SGI(&tsp_svc_params, 0);
		if (res == TEST_RESULT_FAIL)
			return res;

		/* Issue RESUME */
		tsp_svc_params.fid = TSP_FID_RESUME;
		tsp_result = tftf_smc(&tsp_svc_params);

		/* Check the result of the addition */
		if (tsp_result.ret0 != 0 || tsp_result.ret1 != 8 ||
						tsp_result.ret2 != 12) {
			tftf_testcase_printf("SMC resume returned wrong result:"
				     "got %d %d %d expected: 0 8 12\n",
						(unsigned int)tsp_result.ret0,
						(unsigned int)tsp_result.ret1,
						(unsigned int)tsp_result.ret2);
			res = TEST_RESULT_FAIL;
		}

		count++;
	}
	return res;
}

/*
 * @Test_Aim@ Test Secure FIQ when pre-empted by non secure interrupt.
 *
 * We really cannot verify whether FIQ fired and preempted the SGI handler
 * or not. The TSP prints the address at which the execution was interrupted
 * for the FIQ. By looking at the address printed from the TSP logs, we can
 * verify that the SGI handler was interrupted by FIQ. For now, We are assuming
 * CPU is woken by Secure Timer Interrupt.
 *
 * Steps: 1. Issue Standard SMC and preempt it via SGI
 * 2. Wait in the SGI handler for FIQ which is firing every 500 ms.
 * 3. Resume the preempted SMC
 * Returns SUCCESS if above 3 steps are performed correctly else failure.
 */
test_result_t tsp_fiq_while_int(void)
{
	smc_args tsp_svc_params;
	smc_ret_values tsp_result = {0};
	test_result_t res;

	SKIP_TEST_IF_TSP_NOT_PRESENT();

	/* Standard SMC */
	tsp_svc_params.fid = TSP_STD_FID(TSP_ADD);
	tsp_svc_params.arg1 = 4;
	tsp_svc_params.arg2 = 6;
	res = preempt_tsp_via_SGI(&tsp_svc_params, 1);
	if (res == TEST_RESULT_FAIL)
		return res;

	/* Now that we have ensured preemption, issue RESUME */
	tsp_svc_params.fid = TSP_FID_RESUME;
	tsp_result = tftf_smc(&tsp_svc_params);

	/* Check the result of the addition */
	if (tsp_result.ret0 != 0 || tsp_result.ret1 != 8 ||
					tsp_result.ret2 != 12) {
		tftf_testcase_printf("SMC resume returned wrong result:"
			     "got %d %d %d expected: 0 8 12\n",
					(unsigned int)tsp_result.ret0,
					(unsigned int)tsp_result.ret1,
					(unsigned int)tsp_result.ret2);
		return TEST_RESULT_FAIL;
	}
	return TEST_RESULT_SUCCESS;
}
