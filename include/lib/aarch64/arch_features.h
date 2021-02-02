/*
 * Copyright (c) 2020-2021, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef ARCH_FEATURES_H
#define ARCH_FEATURES_H

#include <stdbool.h>

#include <arch_helpers.h>

static inline bool is_armv7_gentimer_present(void)
{
	/* The Generic Timer is always present in an ARMv8-A implementation */
	return true;
}

static inline bool is_armv8_1_pan_present(void)
{
	return ((read_id_aa64mmfr1_el1() >> ID_AA64MMFR1_EL1_PAN_SHIFT) &
		ID_AA64MMFR1_EL1_PAN_MASK) != 0U;
}

static inline bool is_armv8_2_sve_present(void)
{
	return ((read_id_aa64pfr0_el1() >> ID_AA64PFR0_SVE_SHIFT) &
		ID_AA64PFR0_SVE_MASK) == 1U;
}

static inline bool is_armv8_2_ttcnp_present(void)
{
	return ((read_id_aa64mmfr2_el1() >> ID_AA64MMFR2_EL1_CNP_SHIFT) &
		ID_AA64MMFR2_EL1_CNP_MASK) != 0U;
}

static inline bool is_armv8_3_pauth_present(void)
{
	uint64_t mask = (ID_AA64ISAR1_GPI_MASK << ID_AA64ISAR1_GPI_SHIFT) |
			(ID_AA64ISAR1_GPA_MASK << ID_AA64ISAR1_GPA_SHIFT) |
			(ID_AA64ISAR1_API_MASK << ID_AA64ISAR1_API_SHIFT) |
			(ID_AA64ISAR1_APA_MASK << ID_AA64ISAR1_APA_SHIFT);

	/* If any of the fields is not zero, PAuth is present */
	return (read_id_aa64isar1_el1() & mask) != 0U;
}

static inline bool is_armv8_3_pauth_apa_api_present(void)
{
	uint64_t mask = (ID_AA64ISAR1_API_MASK << ID_AA64ISAR1_API_SHIFT) |
			(ID_AA64ISAR1_APA_MASK << ID_AA64ISAR1_APA_SHIFT);

	return (read_id_aa64isar1_el1() & mask) != 0U;
}

static inline bool is_armv8_3_pauth_gpa_gpi_present(void)
{
	uint64_t mask = (ID_AA64ISAR1_GPI_MASK << ID_AA64ISAR1_GPI_SHIFT) |
			(ID_AA64ISAR1_GPA_MASK << ID_AA64ISAR1_GPA_SHIFT);

	return (read_id_aa64isar1_el1() & mask) != 0U;
}

static inline bool is_armv8_4_dit_present(void)
{
	return ((read_id_aa64pfr0_el1() >> ID_AA64PFR0_DIT_SHIFT) &
		ID_AA64PFR0_DIT_MASK) == 1U;
}

static inline bool is_armv8_4_ttst_present(void)
{
	return ((read_id_aa64mmfr2_el1() >> ID_AA64MMFR2_EL1_ST_SHIFT) &
		ID_AA64MMFR2_EL1_ST_MASK) == 1U;
}

static inline bool is_armv8_5_bti_present(void)
{
	return ((read_id_aa64pfr1_el1() >> ID_AA64PFR1_EL1_BT_SHIFT) &
		ID_AA64PFR1_EL1_BT_MASK) == BTI_IMPLEMENTED;
}

static inline unsigned int get_armv8_5_mte_support(void)
{
	return ((read_id_aa64pfr1_el1() >> ID_AA64PFR1_EL1_MTE_SHIFT) &
		ID_AA64PFR1_EL1_MTE_MASK);
}

static inline bool is_armv8_6_fgt_present(void)
{
	return ((read_id_aa64mmfr0_el1() >> ID_AA64MMFR0_EL1_FGT_SHIFT) &
		ID_AA64MMFR0_EL1_FGT_MASK) == ID_AA64MMFR0_EL1_FGT_SUPPORTED;
}

static inline unsigned long int get_armv8_6_ecv_support(void)
{
	return ((read_id_aa64mmfr0_el1() >> ID_AA64MMFR0_EL1_ECV_SHIFT) &
		ID_AA64MMFR0_EL1_ECV_MASK);
}

static inline uint32_t arch_get_debug_version(void)
{
	return ((read_id_aa64dfr0_el1() & ID_AA64DFR0_DEBUG_BITS) >>
		ID_AA64DFR0_DEBUG_SHIFT);
}

#endif /* ARCH_FEATURES_H */
