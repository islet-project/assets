/*
 * Copyright (c) 2020, NVIDIA CORPORATION. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <arch_helpers.h>
#include <debug.h>
#include <tftf_lib.h>
#include <xlat_tables_v2.h>

#include <platform_def.h>

/*******************************************************************************
 * Common Tegra SiP SMCs
 ******************************************************************************/
#define TEGRA_SIP_NEW_VIDEOMEM_REGION		0x82000003ULL

/*
 * @Test_Aim@ Test to issue VideoMem SiP SMC function IDs.
 *
 * This test runs on the lead CPU and issues TEGRA_SIP_NEW_VIDEOMEM_REGION
 * SMC to resize the memory region.
 */
test_result_t test_sip_videomem_resize(void)
{
	uint32_t size_in_bytes = (4U << 20);
	uint32_t offset = (8U << 20);
	uint64_t vidmem_base = DRAM_END, mem;
	uint64_t buf[] = { 0xCAFEBABE, 0xCAFEBABE, 0xCAFEBABE, 0xCAFEBABE };
	smc_args args = { TEGRA_SIP_NEW_VIDEOMEM_REGION, vidmem_base, size_in_bytes };
	smc_ret_values ret;
	test_result_t result = TEST_RESULT_SUCCESS;
	int err;

	/* Map dummy memory region for the test */
	err = mmap_add_dynamic_region(vidmem_base, vidmem_base, size_in_bytes << 2,
		MT_DEVICE | MT_RW | MT_NS | MT_EXECUTE_NEVER);
	if (err != 0) {
		tftf_testcase_printf("%s: could not map memory (%d)\n", __func__, err);
		return TEST_RESULT_FAIL;
	}

	/* copy sample data before setting up the memory protections */
	memcpy((char *)vidmem_base, (void *)buf, sizeof(buf));
	flush_dcache_range(vidmem_base, sizeof(buf));

	/* Issue the SMC to program videomem and expect success */
	ret = tftf_smc(&args);
	if (ret.ret0 != 0UL) {
		tftf_testcase_printf("%s failed. Expected 0, received %ld\n",
			__func__, (long int)ret.ret0);
		result = TEST_RESULT_FAIL;
		goto exit;
	}

	/* copy sample data before setting up the memory protections */
	memcpy((char *)vidmem_base + offset, (void *)buf, sizeof(buf));
	flush_dcache_range(vidmem_base + offset, sizeof(buf));

	/* Issue request to "move" the protected memory region */
	args.arg1 = vidmem_base + offset;
	args.arg2 = (u_register_t)size_in_bytes;
	ret = tftf_smc(&args);
	if (ret.ret0 != 0UL) {
		tftf_testcase_printf("%s failed. Expected 0, received %ld\n",
			__func__, (long int)ret.ret0);
		result = TEST_RESULT_FAIL;
		goto exit;
	}

	/* Verify that memory in the open has been cleared */
	mem = vidmem_base;
	for (unsigned int i = 0U; i < (size_in_bytes / 8); i++, mem += 8) {
		if (*(uint64_t *)(void *)mem != 0ULL) {
			tftf_testcase_printf("%s failed. Memory is non-zero (%llx:%llx)\n",
				__func__, mem, *(uint64_t *)(void *)mem);
			result = TEST_RESULT_FAIL;
			goto exit;
		}
	}

	/* copy sample data before setting up the memory protections */
	memcpy((char *)vidmem_base, (void *)buf, sizeof(buf));
	flush_dcache_range(vidmem_base, sizeof(buf));

	/* Issue request to "move" the protected memory region */
	args.arg1 = vidmem_base;
	args.arg2 = (u_register_t)size_in_bytes;
	ret = tftf_smc(&args);
	if (ret.ret0 != 0UL) {
		tftf_testcase_printf("%s failed. Expected 0, received %ld\n",
			__func__, (long int)ret.ret0);
		result = TEST_RESULT_FAIL;
		goto exit;
	}

	/* Verify that memory in the open has been cleared */
	mem = vidmem_base + offset;
	for (unsigned int i = 0U; i < (size_in_bytes / 8); i++, mem += 8) {
		if (*(uint64_t *)(void *)mem != 0U) {
			tftf_testcase_printf("%s failed. Memory is non-zero (%llx:%llx)\n",
				__func__, mem, *(uint64_t *)(void *)mem);
			result = TEST_RESULT_FAIL;
			goto exit;
		}
	}

exit:
	/* unmap dummy memory region */
	err = mmap_remove_dynamic_region(vidmem_base, size_in_bytes << 2);
	if (err != 0) {
		tftf_testcase_printf("%s: could not unmap memory (%d)\n",
			__func__, err);
		result = TEST_RESULT_FAIL;
	}

	return result;
}

/*
 * @Test_Aim@ Test to issue common SiP SMC function IDs.
 *
 * This test runs on the lead CPU and issues TEGRA_SIP_NEW_VIDEOMEM_REGION
 * and tests positive and negative scenarios.
 */
test_result_t test_sip_videomem_incorrect_inputs(void)
{
	smc_args args = { TEGRA_SIP_NEW_VIDEOMEM_REGION };
	smc_ret_values ret;

	/* Issue the SMC with no input parameters and expect error */
	ret = tftf_smc(&args);
	if (ret.ret0 == 0UL) {
		tftf_testcase_printf("%s failed. Expected -1, received %ld\n",
			__func__, (long int)ret.ret0);
		return TEST_RESULT_FAIL;
	}

	/* Issue the SMC with incorrect parameters and expect error */
	args.arg1 = 0x10000000ULL;
	args.arg2 = 4ULL << 20;

	ret = tftf_smc(&args);
	if (ret.ret0 == 0UL) {
		tftf_testcase_printf("%s failed. Expected -1, received %ld\n",
			__func__, (long int)ret.ret0);
		return TEST_RESULT_FAIL;
	}

	/* Issue the SMC with incorrect parameters and expect error */
	args.arg1 = 0x40000000ULL;
	args.arg2 = 4ULL << 20;

	ret = tftf_smc(&args);
	if (ret.ret0 == 0UL) {
		tftf_testcase_printf("%s failed. Expected -1, received %ld\n",
			__func__, (long int)ret.ret0);
		return TEST_RESULT_FAIL;
	}

	/* Issue the SMC with incorrect parameters and expect error */
	args.arg1 = DRAM_END - (4U << 20);
	args.arg2 = 0x100ULL;

	ret = tftf_smc(&args);
	if (ret.ret0 == 0UL) {
		tftf_testcase_printf("%s failed. Expected -1, received %ld\n",
			__func__, (long int)ret.ret0);
		return TEST_RESULT_FAIL;
	}

	/* Issue the SMC with incorrect parameters and expect error */
	args.arg1 = DRAM_END - (4U << 20);
	args.arg2 = 0ULL;

	ret = tftf_smc(&args);
	if (ret.ret0 == 0UL) {
		tftf_testcase_printf("%s failed. Expected -1, received %ld\n",
			__func__, (long int)ret.ret0);
		return TEST_RESULT_FAIL;
	}

	/* Issue the SMC with incorrect parameters and expect error */
	args.arg1 = 0ULL;
	args.arg2 = 4ULL << 20;

	ret = tftf_smc(&args);
	if (ret.ret0 == 0UL) {
		tftf_testcase_printf("%s failed. Expected -1, received %ld\n",
			__func__, (long int)ret.ret0);
		return TEST_RESULT_FAIL;
	}

	return TEST_RESULT_SUCCESS;
}
