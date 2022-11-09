/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2022 Arm Limited.
 * All rights reserved.
 */
#include <libcflat.h>

#include <asm/pgtable.h>
#include <asm/processor.h>
#include <asm/rsi.h>

bool rsi_present;

int rsi_invoke(unsigned int function_id, unsigned long arg0,
	       unsigned long arg1, unsigned long arg2,
	       unsigned long arg3, unsigned long arg4,
	       unsigned long arg5, unsigned long arg6,
	       unsigned long arg7, unsigned long arg8,
	       unsigned long arg9, unsigned long arg10,
	       struct smccc_result *result)
{
	return arm_smccc_smc(function_id, arg0, arg1, arg2, arg3, arg4, arg5,
			     arg6, arg7, arg8, arg9, arg10, result);
}

struct rsi_realm_config __attribute__((aligned(RSI_GRANULE_SIZE))) config;

static unsigned long rsi_get_realm_config(struct rsi_realm_config *cfg)
{
	struct smccc_result res;

	rsi_invoke(SMC_RSI_REALM_CONFIG, __virt_to_phys((unsigned long)cfg),
		   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, &res);

	return res.r0;
}

int rsi_get_version(void)
{
	struct smccc_result res = {};
	int ret;

	if ((get_id_aa64pfr0_el1() & ID_AA64PFR0_EL1_EL3) == ID_AA64PFR0_EL1_EL3_NI)
		return -1;

	ret = rsi_invoke(SMC_RSI_ABI_VERSION, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		         &res);
	if (ret)
		return ret;

	return res.r0;
}

void arm_rsi_init(void)
{
	if (rsi_get_version() != RSI_ABI_VERSION)
		return;

	if (rsi_get_realm_config(&config))
		return;

	rsi_present = true;

	phys_mask_shift = (config.ipa_width - 1);
	/* Set the upper bit of the IPA as the NS_SHARED pte attribute */
	prot_ns_shared = (1UL << phys_mask_shift);
}
