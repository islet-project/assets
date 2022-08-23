/*
 * Copyright (c) 2021-2022, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef SVE_H
#define SVE_H

#define fill_sve_helper(num) "ldr z"#num", [%0, #"#num", MUL VL];"
#define read_sve_helper(num) "str z"#num", [%0, #"#num", MUL VL];"

/*
 * Max. vector length permitted by the architecture:
 * SVE:	 2048 bits = 256 bytes
 */
#define SVE_VECTOR_LEN_BYTES		256
#define SVE_NUM_VECTORS			32

#ifndef __ASSEMBLY__

typedef uint8_t sve_vector_t[SVE_VECTOR_LEN_BYTES];

#ifdef __aarch64__

/* Returns the SVE implemented VL in bytes (constrained by ZCR_EL3.LEN) */
static inline uint64_t sve_vector_length_get(void)
{
	uint64_t vl;

	__asm__ volatile(
		".arch_extension sve\n"
		"rdvl %0, #1;"
		".arch_extension nosve\n"
		: "=r" (vl)
	);

	return vl;
}

#endif /* __aarch64__ */
#endif /* __ASSEMBLY__ */
#endif /* SVE_H */
