/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2022 Arm Limited.
 * All rights reserved.
 */

#include <libcflat.h>
#include <asm/smp.h>
#include <stdlib.h>

#include <asm/rsi.h>

#define CPU0_ID			0
#define CPU1_ID			(CPU0_ID + 1)
#define CPUS_MAX		(CPU1_ID + 1)
#define RMM_FPU_QREG_MAX	32
#define RMM_FPU_RESULT_PASS	(-1U)

#define fpu_reg_read(val)				\
({							\
	uint64_t *__val = (val);			\
	asm volatile("stp q0, q1, [%0], #32\n\t"	\
		     "stp q2, q3, [%0], #32\n\t"	\
		     "stp q4, q5, [%0], #32\n\t"	\
		     "stp q6, q7, [%0], #32\n\t"	\
		     "stp q8, q9, [%0], #32\n\t"	\
		     "stp q10, q11, [%0], #32\n\t"	\
		     "stp q12, q13, [%0], #32\n\t"	\
		     "stp q14, q15, [%0], #32\n\t"	\
		     "stp q16, q17, [%0], #32\n\t"	\
		     "stp q18, q19, [%0], #32\n\t"	\
		     "stp q20, q21, [%0], #32\n\t"	\
		     "stp q22, q23, [%0], #32\n\t"	\
		     "stp q24, q25, [%0], #32\n\t"	\
		     "stp q26, q27, [%0], #32\n\t"	\
		     "stp q28, q29, [%0], #32\n\t"	\
		     "stp q30, q31, [%0], #32\n\t"	\
		     : "=r" (__val)			\
		     :					\
		     : "q0", "q1", "q2", "q3",		\
			"q4", "q5", "q6", "q7",		\
			"q8", "q9", "q10", "q11",	\
			"q12", "q13", "q14",		\
			"q15", "q16", "q17",		\
			"q18", "q19", "q20",		\
			"q21", "q22", "q23",		\
			"q24", "q25", "q26",		\
			"q27", "q28", "q29",		\
			"q30", "q31", "memory");	\
})

#define fpu_reg_write(val)			\
do {						\
	uint64_t *__val = (val);		\
	asm volatile("ldp q0, q1, [%0]\n\t"	\
		     "ldp q2, q3, [%0]\n\t"	\
		     "ldp q4, q5, [%0]\n\t"	\
		     "ldp q6, q7, [%0]\n\t"	\
		     "ldp q8, q9, [%0]\n\t"	\
		     "ldp q10, q11, [%0]\n\t"	\
		     "ldp q12, q13, [%0]\n\t"	\
		     "ldp q14, q15, [%0]\n\t"	\
		     "ldp q16, q17, [%0]\n\t"	\
		     "ldp q18, q19, [%0]\n\t"	\
		     "ldp q20, q21, [%0]\n\t"	\
		     "ldp q22, q23, [%0]\n\t"	\
		     "ldp q24, q25, [%0]\n\t"	\
		     "ldp q26, q27, [%0]\n\t"	\
		     "ldp q28, q29, [%0]\n\t"	\
		     "ldp q30, q31, [%0]\n\t"	\
		     :				\
		     : "r" (__val)		\
		     : "q0", "q1", "q2", "q3",  \
			"q4", "q5", "q6", "q7", \
			"q8", "q9", "q10", "q11",\
			"q12", "q13", "q14",	\
			"q15", "q16", "q17",	\
			"q18", "q19", "q20",	\
			"q21", "q22", "q23",	\
			"q24", "q25", "q26",	\
			"q27", "q28", "q29",	\
			"q30", "q31", "memory");\
} while (0)

static void nr_cpu_check(int nr)
{
	if (nr_cpus < nr)
		report_abort("At least %d cpus required", nr);
}
/**
 * @brief check if the FPU/SIMD register contents are the same as
 * the input data provided.
 */
static uint32_t __realm_fpuregs_testall(uint64_t *indata)
{
	/* 128b aligned array to read data into */
	uint64_t outdata[RMM_FPU_QREG_MAX * 2]
			 __attribute__((aligned(sizeof(__uint128_t)))) = {
			[0 ... ((RMM_FPU_QREG_MAX * 2) - 1)] = 0 };
	uint8_t regcnt	= 0;
	uint32_t result	= 0;

	if (indata == NULL)
		report_abort("invalid data pointer received");

	/* read data from FPU registers */
	fpu_reg_read(outdata);

	/* check is the data is the same */
	for (regcnt = 0; regcnt < (RMM_FPU_QREG_MAX * 2); regcnt += 2) {
		if ((outdata[regcnt] != indata[regcnt % 4]) ||
			(outdata[regcnt+1] != indata[(regcnt+1) % 4])) {
			report_info(
			"fpu/simd save/restore failed for reg: q%d expected: %lx_%lx received: %lx_%lx\n",
			regcnt / 2, indata[(regcnt+1) % 4],
			indata[regcnt % 4], outdata[regcnt+1],
			outdata[regcnt]);
		} else {
			/* populate a bitmask indicating which
			 * registers passed/failed
			 */
			result |= (1 << (regcnt / 2));
		}
	}

	return result;
}

/**
 * @brief writes randomly sampled data into the FPU/SIMD registers.
 */
static void __realm_fpuregs_writeall_random(uint64_t **indata)
{

	/* allocate 128b aligned memory */
	*indata = memalign(sizeof(__uint128_t), sizeof(uint64_t) * 4);

	/* populate the memory with sampled data from a counter */
	(*indata)[0] = get_cntvct();
	(*indata)[1] = get_cntvct();
	(*indata)[2] = get_cntvct();
	(*indata)[3] = get_cntvct();

	/* write data into FPU registers */
	fpu_reg_write(*indata);
}

static void realm_fpuregs_writeall_run(void *data)
{

	uint64_t **indata	= (uint64_t **)data;

	__realm_fpuregs_writeall_random(indata);
}

static void realm_fpuregs_testall_run(void *data)
{

	uint64_t *indata	= (uint64_t *)data;
	uint32_t result		= 0;

	result = __realm_fpuregs_testall(indata);
	report((result == RMM_FPU_RESULT_PASS),
	       "fpu/simd register save/restore mask: 0x%x", result);
}

/**
 * @brief This test uses two VCPU to test FPU/SIMD save/restore
 * @details REC1 (vcpu1) writes random data into FPU/SIMD
 * registers, REC0 (vcpu0) corrupts/overwrites the data and finally
 * REC1 checks if the data remains unchanged in its context.
 */
static void realm_fpuregs_context_switch_cpu1(void)
{
	int target		= CPU1_ID;
	uint64_t *indata_remote	= NULL;
	uint64_t *indata_local	= NULL;

	/* write data from REC1/VCPU1 */
	on_cpu(target, realm_fpuregs_writeall_run, &indata_remote);

	/* Overwrite from REC0/VCPU0 */
	__realm_fpuregs_writeall_random(&indata_local);

	/* check data consistency */
	on_cpu(target, realm_fpuregs_testall_run, indata_remote);

	free(indata_remote);
	free(indata_local);
}

/**
 * @brief This test uses two VCPU to test FPU/SIMD save/restore
 * @details REC0 (vcpu0) writes random data into FPU/SIMD
 * registers, REC1 (vcpu1) corrupts/overwrites the data and finally
 * REC0 checks if the data remains unchanged in its context.
 */
static void realm_fpuregs_context_switch_cpu0(void)
{

	int target		= CPU1_ID;
	uint64_t *indata_local	= NULL;
	uint64_t *indata_remote	= NULL;
	uint32_t result		= 0;

	/* write data from REC0/VCPU0 */
	__realm_fpuregs_writeall_random(&indata_local);

	/* Overwrite from REC1/VCPU1 */
	on_cpu(target, realm_fpuregs_writeall_run, &indata_remote);

	/* check data consistency */
	result = __realm_fpuregs_testall(indata_local);
	report((result == RMM_FPU_RESULT_PASS),
	       "fpu/simd register save/restore mask: 0x%x", result);

	free(indata_remote);
	free(indata_local);
}
/**
 * checks if during realm context switch, FPU/SIMD registers
 * are saved/restored.
 */
static void realm_fpuregs_context_switch(void)
{

	realm_fpuregs_context_switch_cpu0();
	realm_fpuregs_context_switch_cpu1();
}

int main(int argc, char **argv)
{
	report_prefix_pushf("realm-fpu");

	if (!is_realm())
		report_skip("Not running in Realm world, skipping");

	nr_cpu_check(CPUS_MAX);
	realm_fpuregs_context_switch();

	return report_summary();
}
