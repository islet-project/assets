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
	report_prefix_push("rsi");

	if (!is_realm()) {
		report_skip("Not a realm, skipping tests");
		goto exit;
	}

	rsi_test_version();
exit:
	return report_summary();
}
