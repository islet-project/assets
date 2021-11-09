/*
 * Copyright (c) 2021, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef SPMC_H
#define SPMC_H

#include <ffa_helpers.h>
#include <spm_common.h>

/* Should match with IDs defined in SPM/Hafnium */
#define SPM_INTERRUPT_ENABLE            (0xFF03)
#define SPM_INTERRUPT_GET               (0xFF04)
#define SPM_INTERRUPT_DEACTIVATE	(0xFF08)
#define SPM_DEBUG_LOG                   (0xBD000000)

/*
 * Hypervisor Calls Wrappers
 */

uint32_t spm_interrupt_get(void);
int64_t spm_interrupt_enable(uint32_t int_id, bool enable, enum interrupt_pin pin);
int64_t spm_interrupt_deactivate(uint32_t vint_id);
void spm_debug_log(char c);

#endif /* SPMC_H */
