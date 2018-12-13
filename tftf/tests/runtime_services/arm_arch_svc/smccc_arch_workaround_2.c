/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <arch_helpers.h>
#include <arm_arch_svc.h>
#include <debug.h>
#include <plat_topology.h>
#include <power_management.h>
#include <psci.h>
#include <smccc.h>
#include <string.h>
#include <tftf_lib.h>

#ifdef AARCH64
#define CORTEX_A76_MIDR 0x410fd0b0

static int cortex_a76_test(void);

static struct ent {
	unsigned int midr;
	int (*wa_required)(void);
} entries[] = {
	{ .midr = CORTEX_A76_MIDR, .wa_required = cortex_a76_test },
};

static int cortex_a76_test(void)
{
	return 1;
}

static test_result_t test_smccc_entrypoint(void)
{
	smc_args args;
	smc_ret_values ret;
	int32_t expected_ver;
	unsigned int my_midr, midr_mask;
	int wa_required;
	size_t i;

	/* Check if SMCCC version is at least v1.1 */
	expected_ver = MAKE_SMCCC_VERSION(1, 1);
	memset(&args, 0, sizeof(args));
	args.fid = SMCCC_VERSION;
	ret = tftf_smc(&args);
	if ((int32_t)ret.ret0 < expected_ver) {
		tftf_testcase_printf("Unexpected SMCCC version: 0x%x\n",
		       (int)ret.ret0);
		return TEST_RESULT_SKIPPED;
	}

	/* Check if SMCCC_ARCH_WORKAROUND_2 is required or not */
	memset(&args, 0, sizeof(args));
	args.fid = SMCCC_ARCH_FEATURES;
	args.arg1 = SMCCC_ARCH_WORKAROUND_2;
	ret = tftf_smc(&args);
	if ((int)ret.ret0 == -1) {
		tftf_testcase_printf("SMCCC_ARCH_WORKAROUND_2 is not implemented\n");
		return TEST_RESULT_SKIPPED;
	}

	/* If the call returns 0, it means the workaround is required */
	if ((int)ret.ret0 == 0)
		wa_required = 1;
	else
		wa_required = 0;

	/* Check if the SMC return value matches our expectations */
	my_midr = (unsigned int)read_midr_el1();
	midr_mask = (MIDR_IMPL_MASK << MIDR_IMPL_SHIFT) |
		(MIDR_PN_MASK << MIDR_PN_SHIFT);
	for (i = 0; i < ARRAY_SIZE(entries); i++) {
		struct ent *entp = &entries[i];

		if ((my_midr & midr_mask) == (entp->midr & midr_mask)) {
			if (entp->wa_required() != wa_required)
				return TEST_RESULT_FAIL;
			break;
		}
	}
	if (i == ARRAY_SIZE(entries) && wa_required) {
		tftf_testcase_printf("TFTF workaround table out of sync with TF\n");
		return TEST_RESULT_FAIL;
	}

	/* Invoke the workaround to make sure nothing nasty happens */
	memset(&args, 0, sizeof(args));
	args.fid = SMCCC_ARCH_WORKAROUND_2;
	tftf_smc(&args);
	return TEST_RESULT_SUCCESS;
}

test_result_t test_smccc_arch_workaround_2(void)
{
	u_register_t lead_mpid, target_mpid;
	int cpu_node, ret;

	lead_mpid = read_mpidr_el1() & MPID_MASK;

	/* Power on all the non-lead cores. */
	for_each_cpu(cpu_node) {
		target_mpid = tftf_get_mpidr_from_node(cpu_node);
		if (lead_mpid == target_mpid)
			continue;
		ret = tftf_cpu_on(target_mpid,
		    (uintptr_t)test_smccc_entrypoint, 0);
		if (ret != PSCI_E_SUCCESS) {
			ERROR("CPU ON failed for 0x%llx\n",
			    (unsigned long long)target_mpid);
			return TEST_RESULT_FAIL;
		}
		/*
		 * Wait for test_smccc_entrypoint to return
		 * and the CPU to power down
		 */
		while (tftf_psci_affinity_info(target_mpid, MPIDR_AFFLVL0) !=
			PSCI_STATE_OFF)
			continue;
	}

	return test_smccc_entrypoint();
}
#else
test_result_t test_smccc_arch_workaround_2(void)
{
	INFO("%s skipped on AArch32\n", __func__);
	return TEST_RESULT_SKIPPED;
}
#endif
