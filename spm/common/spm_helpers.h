/*
 * Copyright (c) 2021, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef SPMC_H
#define SPMC_H

#include <ffa_helpers.h>

/* Should match with IDs defined in SPM/Hafnium */
#define SPM_INTERRUPT_GET               (0xFF04)
#define SPM_DEBUG_LOG                   (0xBD000000)

/*
 * Hypervisor Calls Wrappers
 */

uint32_t spm_interrupt_get(void);
void spm_debug_log(char c);

#endif /* SPMC_H */
