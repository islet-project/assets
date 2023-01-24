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

/*
 * Logical representation of return code returned by RMM commands.
 * Each failure mode of a given command should return a unique return code, so
 * that the caller can use it to unambiguously identify the failure mode.  To
 * avoid having a very large list of enumerated values, the return code is
 * composed of a status which identifies the category of the error (for example,
 * an address was misaligned), and an index which disambiguates between multiple
 * similar failure modes (for example, a command may take multiple addresses as
 * its input; the index identifies _which_ of them was misaligned.)
 */
typedef unsigned int status_t;
typedef struct {
	status_t status;
	unsigned int index;
} return_code_t;

/*
 * Convenience function for creating a return_code_t.
 */
static inline return_code_t make_return_code(unsigned int status,
					     unsigned int index)
{
	return (return_code_t) {status, index};
}

/*
 * Unpacks a return code.
 */
static inline return_code_t unpack_return_code(unsigned long error_code)
{
	return make_return_code(error_code & 0xff, error_code >> 8);
}

void arm_rsi_init(void);

int rsi_invoke(unsigned int function_id, unsigned long arg0,
	       unsigned long arg1, unsigned long arg2,
	       unsigned long arg3, unsigned long arg4,
	       unsigned long arg5, unsigned long arg6,
	       unsigned long arg7, unsigned long arg8,
	       unsigned long arg9, unsigned long arg10,
	       struct smccc_result *result);

int rsi_get_version(void);
void rsi_attest_token_init(phys_addr_t addr, unsigned long *challenge,
			   struct smccc_result *res);
void rsi_attest_token_continue(phys_addr_t addr, struct smccc_result *res);
void rsi_extend_measurement(unsigned int index, unsigned long size,
			    unsigned long *measurement,
			    struct smccc_result *res);
void rsi_read_measurement(unsigned int index, struct smccc_result *res);

static inline bool is_realm(void)
{
	return rsi_present;
}

enum ripas_t {
	RIPAS_EMPTY,
	RIPAS_RAM,
};

void arm_set_memory_protected(unsigned long va, size_t size);
void arm_set_memory_shared(unsigned long va, size_t size);

#endif /* __ASMARM64_RSI_H_ */
