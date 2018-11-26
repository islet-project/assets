/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef BL1_H
#define BL1_H

#define BL1_SMC_CALL_COUNT		0x0
#define BL1_SMC_UID			0x1
/* SMC #0x2 reserved. */
#define BL1_SMC_VERSION			0x3

/* SMC function ID used to request BL1 to execute BL31. */
#define BL1_SMC_RUN_IMAGE		0x4

/* SMC function IDs for Firmware Update operations. */
#define FWU_SMC_IMAGE_COPY		0x10
#define FWU_SMC_IMAGE_AUTH		0x11
#define FWU_SMC_IMAGE_EXECUTE		0x12
#define FWU_SMC_IMAGE_RESUME		0x13
#define FWU_SMC_SEC_IMAGE_DONE		0x14
#define FWU_SMC_UPDATE_DONE		0x15
#define FWU_SMC_IMAGE_RESET		0x16

/*
 * Number of SMC calls supported in BL1.
 *
 * Note that when Trusted Board Boot is disabled in BL1, this drops down to 4,
 * as the 7 FWU SMCs are not implemented. We test BL1's SMC interface only in
 * the context of FWU tests, where TBB is enabled.
 */
#define BL1_NUM_SMC_CALLS	11

/* Version reported by the BL1_SMC_VERSION SMC. */
#define BL1_VERSION		0x1

#endif /* BL1_H */
