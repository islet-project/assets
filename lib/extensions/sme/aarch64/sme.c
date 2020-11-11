/*
 * Copyright (c) 2021, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdbool.h>
#include <stdio.h>

#include <arch.h>
#include <arch_helpers.h>
#include <lib/extensions/sme.h>

#ifdef __aarch64__

/*
 * feat_sme_supported
 *   Check if SME is supported on this platform.
 * Return
 *   true if SME supported, false if not.
 */
bool feat_sme_supported(void)
{
	uint64_t features;

	features = read_id_aa64pfr1_el1() >> ID_AA64PFR1_EL1_SME_SHIFT;
	return (features & ID_AA64PFR1_EL1_SME_MASK) != 0U;
}

/*
 * feat_sme_fa64_supported
 *  Check if FEAT_SME_FA64 is supported.
 * Return
 *  True if supported, false if not.
 */
bool feat_sme_fa64_supported(void)
{
	uint64_t features;

	features = read_id_aa64smfr0_el1();
	return (features & ID_AA64SMFR0_EL1_FA64_BIT) != 0U;
}

/*
 * sme_enable
 *   Enable SME for nonsecure use at EL2 for TFTF cases.
 * Return
 *   0 if successful.
 */
int sme_enable(void)
{
	u_register_t reg;

	/* Make sure SME is supported. */
	if (!feat_sme_supported()) {
		return -1;
	}

	/*
	 * Make sure SME accesses don't cause traps by setting appropriate fields
	 * in CPTR_EL2.
	 */
	reg = read_cptr_el2();
	if ((read_hcr_el2() & HCR_E2H_BIT) == 0U) {
		/* When HCR_EL2.E2H == 0, clear TSM bit in CPTR_EL2. */
		reg = reg & ~CPTR_EL2_TSM_BIT;
	} else {
		/* When HCR_EL2.E2H == 1, set SMEN bits in CPTR_EL2. */
		reg = reg | (CPTR_EL2_SMEN_MASK << CPTR_EL2_SMEN_SHIFT);
	}
	write_cptr_el2(reg);

	return 0;
}

/*
 * sme_smstart
 *   This function enables streaming mode and optinally enables ZA array access
 *   at the same time.
 * Parameters
 *   enable_za: If set, ZA access is enabled.  If cleared, ZA bit is untouched.
 */
void sme_smstart(bool enable_za)
{
	u_register_t svcr = SVCR_SM_BIT;

	if (enable_za) {
		svcr |= SVCR_ZA_BIT;
	}

	write_svcr(read_svcr() | svcr);
}

/*
 * sme_smstop
 *   This function disables streaming mode OR disables ZA array access but not
 *   both.  It might seem strange but this is the functionality of the SMSTOP
 *   assembly instruction.
 * Parameters
 *   disable_za: If set, ZA access is disabled but streaming mode is not
 *               affected.  If clear, streaming mode is exited and ZA bit is
 *               left alone.
 */
void sme_smstop(bool disable_za)
{
	u_register_t svcr;

	if (disable_za) {
		svcr = ~SVCR_ZA_BIT;
	} else {
		svcr = ~SVCR_SM_BIT;
	}

	write_svcr(read_svcr() & svcr);
}

#endif /* __aarch64__ */
