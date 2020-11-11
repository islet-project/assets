/*
 * Copyright (c) 2021, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef AMU_H
#define AMU_H

#define SME_SMCR_LEN_MAX U(0x1FF)

bool feat_sme_supported(void);
bool feat_sme_fa64_supported(void);
int sme_enable(void);
void sme_smstart(bool enable_za);
void sme_smstop(bool disable_za);

/* Assembly function prototypes */
uint64_t sme_rdvl_1(void);
void sme_try_illegal_instruction(void);

#endif /* AMU_H */
