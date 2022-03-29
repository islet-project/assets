/*
 * Copyright (c) 2020-2022, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <drivers/arm/pl011.h>
#include <drivers/console.h>
#include <ffa_helpers.h>
#include <sp_debug.h>
#include <spm_helpers.h>

static int (*putc_impl)(int);

static int putc_hypcall(int c)
{
	spm_debug_log((char)c);

	return c;
}

static int putc_svccall(int c)
{
	struct ffa_value args = {
		.fid = SPM_DEBUG_LOG,
		.arg1 = c
	};
	ffa_svc(&args);

	return c;
}

static int putc_uart(int c)
{
	console_pl011_putc(c);

	return c;
}

void set_putc_impl(enum stdout_route route)
{
	switch (route) {

	case HVC_CALL_AS_STDOUT:
		putc_impl = putc_hypcall;
		return;

	case SVC_CALL_AS_STDOUT:
		putc_impl = putc_svccall;
		return;

	case PL011_AS_STDOUT:
	default:
		break;
	}

	putc_impl = putc_uart;
}

int console_putc(int c)
{
	if (!putc_impl) {
		return -1;
	}

	return putc_impl(c);
}
