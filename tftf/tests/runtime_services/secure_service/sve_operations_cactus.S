/*
 * Copyright (c) 2022, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <asm_macros.S>

#ifdef __aarch64__
#if __GNUC__ > 8 || (__GNUC__ == 8 && __GNUC_MINOR__ > 0)

#define SVE_ARRAYSIZE 1024

/*
 * Based on example code from the Arm Compiler Scalable Vector Extension User
 * Guide[1].
 * [1] https://developer.arm.com/docs/100891/latest/getting-started-with-the-sve-compiler/compiling-c-and-c-code-for-sve-enabled-targets
 */

	.arch armv8.2-a+crc+fp16+sve
	.global	sve_subtract_interleaved_smc
func sve_subtract_interleaved_smc
	mov	x4, SVE_ARRAYSIZE
	mov	x5, x4
	mov	x3, 0
	whilelo	p0.s, xzr, x4
.loop:
	ld1w	z0.s, p0/z, [x1, x3, lsl 2]
	ld1w	z1.s, p0/z, [x2, x3, lsl 2]
	sub	z0.s, z0.s, z1.s
	st1w	z0.s, p0, [x0, x3, lsl 2]
	incw	x3

	stp x0, x1, [sp, #-48]!
	stp x2, x3, [sp, #16]
	stp x4, x5, [sp, #32]

	/*
	 * Forge a FF-A direct request with a command for cactus to fill SIMD
	 * vectors in the secure world.
	 */
	mov w0, #0x6f                   /* FFA_MSG_SEND_DIRECT_REQ_SMC32 */
	movk w0, #0x8400, lsl #16
	mov     x1, #0x8001             /* src: nwd, dest: SP1 */
	mov     x2, xzr
	mov     x3, #0x4d44
	movk    w3, #0x5349, lsl #16    /* CACTUS_REQ_SIMD_FILL_CMD */
	smc     #0
	and     w1, w0, #0xffff
	cmp     w1, #0x70               /* FFA_MSG_SEND_DIRECT_RESP_SMC32 (low 16bits) */
	bne     .			/* Test hangs if direct response not received */
	cmp	w3, #0x0		/* Check CACTUS_SUCCESS (0x0) returned */
	bne	.
	ldp     x4, x5, [sp, #32]
	ldp     x2, x3, [sp, #16]
	ldp     x0, x1, [sp], #48

	whilelo	p0.s, x3, x5
	bne	.loop
	ret
endfunc sve_subtract_interleaved_smc

#endif /* __GNUC__ > 8 || (__GNUC__ == 8 && __GNUC_MINOR__ > 0) */
#endif /* __aarch64__ */
