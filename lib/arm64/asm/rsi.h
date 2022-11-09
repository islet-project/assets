/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2022 Arm Limited.
 * All rights reserved.
 */
#ifndef __ASMARM64_RSI_H_
#define __ASMARM64_RSI_H_

#include <stdbool.h>

#include <asm/arm-smccc.h>
#include <asm/io.h>
#include <asm/smc-rsi.h>

#define RSI_GRANULE_SIZE	SZ_4K

extern bool rsi_present;

void arm_rsi_init(void);

int rsi_invoke(unsigned int function_id, unsigned long arg0,
	       unsigned long arg1, unsigned long arg2,
	       unsigned long arg3, unsigned long arg4,
	       unsigned long arg5, unsigned long arg6,
	       unsigned long arg7, unsigned long arg8,
	       unsigned long arg9, unsigned long arg10,
	       struct smccc_result *result);

int rsi_get_version(void);

static inline bool is_realm(void)
{
	return rsi_present;
}

#endif /* __ASMARM64_RSI_H_ */
