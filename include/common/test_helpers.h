/*
 * Copyright (c) 2018-2019, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __TEST_HELPERS_H__
#define __TEST_HELPERS_H__

#include <arch_features.h>
#include <plat_topology.h>
#include <psci.h>
#include <spci_svc.h>
#include <tftf_lib.h>
#include <trusted_os.h>
#include <tsp.h>
#include <uuid.h>
#include <uuid_utils.h>

typedef struct {
	uintptr_t addr;
	size_t size;
	unsigned int attr;
	void *arg;
} map_args_unmap_t;

typedef test_result_t (*test_function_arg_t)(void *arg);

#ifdef AARCH32
#define SKIP_TEST_IF_AARCH32()							\
	do {									\
		tftf_testcase_printf("Test not supported on aarch32\n");	\
		return TEST_RESULT_SKIPPED;					\
	} while (0)
#else
#define SKIP_TEST_IF_AARCH32()
#endif

#define SKIP_TEST_IF_LESS_THAN_N_CLUSTERS(n)					\
	do {									\
		unsigned int clusters_cnt;					\
		clusters_cnt = tftf_get_total_clusters_count();			\
		if (clusters_cnt < (n)) {					\
			tftf_testcase_printf(					\
				"Need at least %u clusters, only found %u\n",	\
				(n), clusters_cnt);				\
			return TEST_RESULT_SKIPPED;				\
		}								\
	} while (0)

#define SKIP_TEST_IF_LESS_THAN_N_CPUS(n)					\
	do {									\
		unsigned int cpus_cnt;						\
		cpus_cnt = tftf_get_total_cpus_count();				\
		if (cpus_cnt < (n)) {						\
			tftf_testcase_printf(					\
				"Need at least %u CPUs, only found %u\n",	\
				(n), cpus_cnt);					\
			return TEST_RESULT_SKIPPED;				\
		}								\
	} while (0)

#define SKIP_TEST_IF_TRUSTED_OS_NOT_PRESENT()					\
	do {									\
		uuid_t tos_uuid;						\
										\
		if (!is_trusted_os_present(&tos_uuid)) {			\
			tftf_testcase_printf("No Trusted OS detected\n");	\
			return TEST_RESULT_SKIPPED;				\
		}								\
	} while (0)

#define SKIP_TEST_IF_TSP_NOT_PRESENT()						\
	do {									\
		uuid_t tos_uuid;						\
		char tos_uuid_str[UUID_STR_SIZE];				\
										\
		if (!is_trusted_os_present(&tos_uuid)) {			\
			tftf_testcase_printf("No Trusted OS detected\n");	\
			return TEST_RESULT_SKIPPED;				\
		}								\
										\
		if (!uuid_equal(&tos_uuid, &tsp_uuid)) {			\
			tftf_testcase_printf(					\
				"Trusted OS is not the TSP, its UUID is: %s\n",	\
				uuid_to_str(&tos_uuid, tos_uuid_str));		\
			return TEST_RESULT_SKIPPED;				\
		}								\
	} while (0)

#define SKIP_TEST_IF_PAUTH_NOT_SUPPORTED()					\
	do {									\
		if (!is_armv8_3_pauth_present()) {				\
			tftf_testcase_printf(					\
				"Pointer Authentication not supported\n");	\
			return TEST_RESULT_SKIPPED;				\
		}								\
	} while (0)

#define SKIP_TEST_IF_MM_NOT_PRESENT()						\
	do {									\
		smc_args version_smc = { MM_VERSION_AARCH32 };			\
		smc_ret_values smc_ret = tftf_smc(&version_smc);		\
		uint32_t version = smc_ret.ret0;				\
										\
		if (version == (uint32_t) SMC_UNKNOWN) {			\
			tftf_testcase_printf("SPM not detected.\n");		\
			return TEST_RESULT_SKIPPED;				\
		}								\
	} while (0)

#define SKIP_TEST_IF_MM_VERSION_LESS_THAN(major, minor)				\
	do {									\
		smc_args version_smc = { MM_VERSION_AARCH32 };			\
		smc_ret_values smc_ret = tftf_smc(&version_smc);		\
		uint32_t version = smc_ret.ret0;				\
										\
		if (version == (uint32_t) SMC_UNKNOWN) {			\
			tftf_testcase_printf("SPM not detected.\n");		\
			return TEST_RESULT_SKIPPED;				\
		}								\
										\
		if (version < MM_VERSION_FORM(major, minor)) {			\
			tftf_testcase_printf("MM_VERSION returned %d.%d\n"	\
					     "The required version is %d.%d\n",	\
					     version >> MM_VERSION_MAJOR_SHIFT,	\
					     version & MM_VERSION_MINOR_MASK,	\
					     major, minor);			\
			return TEST_RESULT_SKIPPED;				\
		}								\
										\
		VERBOSE("MM_VERSION returned %d.%d\n",				\
			version >> MM_VERSION_MAJOR_SHIFT,			\
			version & MM_VERSION_MINOR_MASK);			\
	} while (0)

#define SKIP_TEST_IF_SPCI_VERSION_LESS_THAN(major, minor)			\
	do {									\
		smc_args version_smc = { SPCI_VERSION };			\
		smc_ret_values smc_ret = tftf_smc(&version_smc);		\
		uint32_t version = smc_ret.ret0;				\
										\
		if (version == SMC_UNKNOWN) {					\
			tftf_testcase_printf("SPM not detected.\n");		\
			return TEST_RESULT_SKIPPED;				\
		}								\
										\
		if (version < SPCI_VERSION_FORM(major, minor)) {		\
			tftf_testcase_printf("SPCI_VERSION returned %d.%d\n"	\
					     "The required version is %d.%d\n",	\
					     version >> SPCI_VERSION_MAJOR_SHIFT,\
					     version & SPCI_VERSION_MINOR_MASK,	\
					     major, minor);			\
			return TEST_RESULT_SKIPPED;				\
		}								\
	} while (0)

#define SKIP_TEST_IF_ARCH_DEBUG_VERSION_LESS_THAN(version)			\
	do {									\
		uint32_t debug_ver = read_id_aa64dfr0_el1() &			\
			(ID_AA64DFR0_DEBUG_MASK << ID_AA64DFR0_DEBUG_SHIFT);	\
										\
		if ((debug_ver >> ID_AA64DFR0_DEBUG_SHIFT) < version) {		\
			tftf_testcase_printf("Debug version returned %d\n"	\
					     "The required version is %d\n",	\
					     debug_ver >> ID_AA64DFR0_DEBUG_SHIFT,\
					     version);				\
			return TEST_RESULT_SKIPPED;				\
		}								\
	} while (0)

/* Helper macro to verify if system suspend API is supported */
#define is_psci_sys_susp_supported()	\
		(tftf_get_psci_feature_info(SMC_PSCI_SYSTEM_SUSPEND)	\
					== PSCI_E_SUCCESS)

/* Helper macro to verify if PSCI_STAT_COUNT API is supported */
#define is_psci_stat_count_supported()	\
		(tftf_get_psci_feature_info(SMC_PSCI_STAT_COUNT)	\
					== PSCI_E_SUCCESS)

/*
 * Helper function to verify the system state is ready for system
 * suspend. i.e., a single CPU is running and all other CPUs are powered off.
 * Returns 1 if the system is ready to suspend, 0 otherwise.
 */
int is_sys_suspend_state_ready(void);

/*
 * Helper function to reset the system. This function shouldn't return.
 * It is not marked with __dead to help the test to catch some error in
 * TF
 */
void psci_system_reset(void);

/*
 * Helper function that enables/disables the mem_protect mechanism
 */
int psci_mem_protect(int val);


/*
 * Helper function to call PSCI MEM_PROTECT_CHECK
 */
int psci_mem_protect_check(uintptr_t addr, size_t size);


/*
 * Helper function to get a sentinel address that can be used to test mem_protect
 */
unsigned char *psci_mem_prot_get_sentinel(void);

/*
 * Helper function to memory map and unmap a region needed by a test.
 *
 * Return TEST_RESULT_FAIL if the memory could not be successfully mapped or
 * unmapped. Otherwise, return the test functions's result.
 */
test_result_t map_test_unmap(const map_args_unmap_t *args,
			     test_function_arg_t test);

#endif /* __TEST_HELPERS_H__ */
