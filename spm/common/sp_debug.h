/*
 * Copyright (c) 2021, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

enum stdout_route {
	PL011_AS_STDOUT = 0,
	HVC_CALL_AS_STDOUT,
};

void set_putc_impl(enum stdout_route);
