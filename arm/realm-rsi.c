/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2022 Arm Limited.
 * All rights reserved.
 */

#include <libcflat.h>
#include <asm/io.h>
#include <asm/page.h>
#include <asm/processor.h>
#include <asm/psci.h>
#include <alloc_page.h>
#include <asm/rsi.h>
#include <asm/pgtable.h>
#include <asm/processor.h>

#define FID_SMCCC_VERSION	0x80000000
#define FID_INVALID		0xc5000041

#define SMCCC_VERSION_1_1	0x10001
#define SMCCC_SUCCESS		0
#define SMCCC_NOT_SUPPORTED	-1

static bool unknown_taken;

static void unknown_handler(struct pt_regs *regs, unsigned int esr)
{
	report_info("unknown_handler: esr=0x%x", esr);
	unknown_taken = true;
}

static void hvc_call(unsigned int fid)
{
	struct smccc_result res;

	unknown_taken = false;
	arm_smccc_hvc(fid, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, &res);

	if (unknown_taken) {
		report(true, "FID=0x%x caused Unknown exception", fid);
	} else {
		report(false, "FID=0x%x did not cause Unknown exception", fid);
		report_info("x0:  0x%lx", res.r0);
		report_info("x1:  0x%lx", res.r1);
		report_info("x2:  0x%lx", res.r2);
		report_info("x3:  0x%lx", res.r3);
		report_info("x4:  0x%lx", res.r4);
		report_info("x5:  0x%lx", res.r5);
		report_info("x6:  0x%lx", res.r6);
		report_info("x7:  0x%lx", res.r7);
	}
}

static void rsi_test_hvc(void)
{
	report_prefix_push("hvc");

	/* Test that HVC causes Undefined exception, regardless of FID */
	install_exception_handler(EL1H_SYNC, ESR_EL1_EC_UNKNOWN, unknown_handler);
	hvc_call(FID_SMCCC_VERSION);
	hvc_call(FID_INVALID);
	install_exception_handler(EL1H_SYNC, ESR_EL1_EC_UNKNOWN, NULL);

	report_prefix_pop();
}

static void host_call(unsigned int fid, unsigned long expected_x0)
{
	struct smccc_result res;
	struct rsi_host_call __attribute__((aligned(256))) host_call_data = { 0 };

	host_call_data.gprs[0] = fid;

	arm_smccc_smc(SMC_RSI_HOST_CALL, virt_to_phys(&host_call_data),
		       0, 0, 0, 0, 0, 0, 0, 0, 0, 0, &res);

	if (res.r0) {
		report(false, "RSI_HOST_CALL returned 0x%lx", res.r0);
	} else {
		if (host_call_data.gprs[0] == expected_x0) {
			report(true, "FID=0x%x x0=0x%lx",
				fid, host_call_data.gprs[0]);
		} else {
			report(false, "FID=0x%x x0=0x%lx expected=0x%lx",
				fid, host_call_data.gprs[0], expected_x0);
			report_info("x1:  0x%lx", host_call_data.gprs[1]);
			report_info("x2:  0x%lx", host_call_data.gprs[2]);
			report_info("x3:  0x%lx", host_call_data.gprs[3]);
			report_info("x4:  0x%lx", host_call_data.gprs[4]);
			report_info("x5:  0x%lx", host_call_data.gprs[5]);
			report_info("x6:  0x%lx", host_call_data.gprs[6]);
		}
	}
}

static void rsi_test_host_call(void)
{
	report_prefix_push("host_call");

	/* Test that host calls return expected values */
	host_call(FID_SMCCC_VERSION, SMCCC_VERSION_1_1);
	host_call(FID_INVALID, SMCCC_NOT_SUPPORTED);

	report_prefix_pop();
}

static void rsi_test_version(void)
{
	int version;

	report_prefix_push("version");

	version = rsi_get_version();
	if (version < 0) {
		report(false, "SMC_RSI_ABI_VERSION failed (%d)", version);
		return;
	}

	report(version == RSI_ABI_VERSION, "RSI ABI version %u.%u (expected: %u.%u)",
	       RSI_ABI_VERSION_GET_MAJOR(version),
	       RSI_ABI_VERSION_GET_MINOR(version),
	       RSI_ABI_VERSION_GET_MAJOR(RSI_ABI_VERSION),
	       RSI_ABI_VERSION_GET_MINOR(RSI_ABI_VERSION));
	report_prefix_pop();
}

int main(int argc, char **argv)
{
	int i;

	report_prefix_push("rsi");

	if (!is_realm()) {
		report_skip("Not a realm, skipping tests");
		goto exit;
	}

	if (argc < 2) {
		rsi_test_version();
		rsi_test_host_call();
		rsi_test_hvc();
	} else {
		for (i = 1; i < argc; i++) {
			if (strcmp(argv[i], "version") == 0) {
				rsi_test_version();
			} else if (strcmp(argv[i], "hvc") == 0) {
				rsi_test_hvc();
			} else if (strcmp(argv[i], "host_call") == 0) {
				rsi_test_host_call();
			} else {
				report_abort("Unknown subtest '%s'", argv[1]);
			}
		}
	}
exit:
	return report_summary();
}
