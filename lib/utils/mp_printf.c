/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <arch_helpers.h>
#include <platform.h>
#include <spinlock.h>
#include <stdarg.h>
#include <stdio.h>

/* Lock to avoid concurrent accesses to the serial console */
static spinlock_t printf_lock;

/*
 * Print the MPID header, e.g.: [cpu 0x0100]
 *
 * If SHELL_COLOR == 1, this also prints shell's color escape sequences to ease
 * identifying which CPU displays the message. There are 8 standard colors so
 * if the platform has more than 8 CPUs, some colors will be reused.
 */
#if SHELL_COLOR
#define PRINT_MPID_HDR(_mpid)						\
	do {								\
		unsigned int linear_id = platform_get_core_pos(_mpid);	\
		printf("\033[1;%u;40m", 30 + (linear_id & 0x7));	\
		printf("[cpu 0x%.4x] ", _mpid);				\
		printf("\033[0m");					\
	} while (0)
#else
#define PRINT_MPID_HDR(_mpid)						\
	printf("[cpu 0x%.4x] ", _mpid)
#endif /* SHELL_COLOR */

void mp_printf(const char *fmt, ...)
{
	/*
	 * As part of testing Firmware Update feature on Cortex-A57 CPU, an
	 * issue was discovered while printing in NS_BL1U stage. The issue
	 * appears when the second call to `NOTICE()` is made in the
	 * `ns_bl1u_main()`. As a result of this issue the CPU hangs and the
	 * debugger is also not able to connect anymore.
	 *
	 * After further debugging and experiments it was found that if
	 * `read_mpidr_el1()` is avoided or volatile qualifier is used for
	 * reading the mpidr, this issue gets resolved.
	 *
	 * NOTE: The actual/real reason why this happens is still not known.
	 * Moreover this problem is not encountered on Cortex-A53 CPU.
	 */
	volatile unsigned int mpid = read_mpidr_el1() & 0xFFFF;

	va_list ap;
	va_start(ap, fmt);

	spin_lock(&printf_lock);
	PRINT_MPID_HDR(mpid);
	vprintf(fmt, ap);
	spin_unlock(&printf_lock);

	va_end(ap);
}
