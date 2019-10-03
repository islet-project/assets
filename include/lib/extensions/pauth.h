/*
 * Copyright (c) 2019, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef PAUTH_H
#define PAUTH_H

#include <stdint.h>

#ifdef __aarch64__
/* Initialize 128-bit ARMv8.3-PAuth key */
uint128_t init_apkey(void);

/* Program APIAKey_EL1 key and enable ARMv8.3-PAuth */
void pauth_init_enable(void);
#endif	/* __aarch64__ */

#endif /* PAUTH_H */
