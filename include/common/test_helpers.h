/*
 * Copyright (c) 2018-2022, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef TEST_HELPERS_H__
#define TEST_HELPERS_H__

#include <arch_features.h>
#include <ffa_svc.h>
#include <events.h>
#include <plat_topology.h>
#include <psci.h>
#include <spm_common.h>
#include <tftf_lib.h>
#include <trusted_os.h>
#include <tsp.h>
#include <uuid_utils.h>
#include <uuid.h>

typedef struct {
	uintptr_t addr;
	size_t size;
	unsigned int attr;
	void *arg;
} map_args_unmap_t;

typedef test_result_t (*test_function_arg_t)(void *arg);

#ifndef __aarch64__
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

#define SKIP_TEST_IF_DIT_NOT_SUPPORTED()					\
	do {									\
		if (!is_armv8_4_dit_present()) {				\
			tftf_testcase_printf(					\
				"DIT not supported\n");				\
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

#define SKIP_TEST_IF_FGT_NOT_SUPPORTED()					\
	do {									\
		if (!is_armv8_6_fgt_present()) {				\
			tftf_testcase_printf(					\
				"Fine Grained Traps not supported\n");		\
			return TEST_RESULT_SKIPPED;				\
		}								\
	} while (0)

#define SKIP_TEST_IF_SVE_NOT_SUPPORTED()					\
	do {									\
		if (!is_armv8_2_sve_present()) {				\
			tftf_testcase_printf("SVE not supported\n");		\
			return TEST_RESULT_SKIPPED;				\
		}								\
	} while (0)

#define SKIP_TEST_IF_ECV_NOT_SELF_SYNC()					\
	do {									\
		if (get_armv8_6_ecv_support() !=				\
			ID_AA64MMFR0_EL1_ECV_SELF_SYNCH) {			\
			tftf_testcase_printf("ARMv8.6-ECV not supported\n");	\
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

#define SKIP_TEST_IF_MTE_SUPPORT_LESS_THAN(n)					\
	do {									\
		if (get_armv8_5_mte_support() < (n)) {				\
			tftf_testcase_printf(					\
				"Memory Tagging Extension not supported\n");	\
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
			tftf_testcase_printf("MM_VERSION returned %u.%u\n"	\
					"The required version is %u.%u\n",	\
					version >> MM_VERSION_MAJOR_SHIFT,	\
					version & MM_VERSION_MINOR_MASK,	\
					major, minor);				\
			return TEST_RESULT_SKIPPED;				\
		}								\
										\
		VERBOSE("MM_VERSION returned %u.%u\n",				\
			version >> MM_VERSION_MAJOR_SHIFT,			\
			version & MM_VERSION_MINOR_MASK);			\
	} while (0)

#define SKIP_TEST_IF_FFA_VERSION_LESS_THAN(major, minor)			\
	do {									\
		smc_ret_values smc_ret = ffa_version(				\
					MAKE_FFA_VERSION(major, minor));	\
		uint32_t version = smc_ret.ret0;				\
										\
		if (version == FFA_ERROR_NOT_SUPPORTED) {			\
			tftf_testcase_printf("FFA_VERSION not supported.\n");	\
			return TEST_RESULT_SKIPPED;				\
		}								\
										\
		if ((version & FFA_VERSION_BIT31_MASK) != 0U) {				\
			tftf_testcase_printf("FFA_VERSION bad response: %x\n",	\
					version);				\
			return TEST_RESULT_FAIL;				\
		}								\
										\
		if (version < MAKE_FFA_VERSION(major, minor)) {			\
			tftf_testcase_printf("FFA_VERSION returned %u.%u\n"	\
					"The required version is %u.%u\n",	\
					version >> FFA_VERSION_MAJOR_SHIFT,	\
					version & FFA_VERSION_MINOR_MASK,	\
					major, minor);				\
			return TEST_RESULT_SKIPPED;				\
		}								\
	} while (0)

#define SKIP_TEST_IF_ARCH_DEBUG_VERSION_LESS_THAN(version)			\
	do {									\
		uint32_t debug_ver = arch_get_debug_version();			\
										\
		if (debug_ver < version) {					\
			tftf_testcase_printf("Debug version returned %d\n"	\
					     "The required version is %d\n",	\
					     debug_ver,				\
					     version);				\
			return TEST_RESULT_SKIPPED;				\
		}								\
	} while (0)

#define SKIP_TEST_IF_FFA_ENDPOINT_NOT_DEPLOYED(mb, ffa_uuid)			\
	do {									\
		smc_ret_values smc_ret = ffa_partition_info_get(ffa_uuid);	\
		ffa_rx_release();						\
		if (ffa_func_id(smc_ret) == FFA_ERROR && 			\
		    ffa_error_code(smc_ret) == FFA_ERROR_INVALID_PARAMETER) {	\
			tftf_testcase_printf("FFA endpoint not deployed!\n");	\
			return TEST_RESULT_SKIPPED;				\
		} else if (smc_ret.ret0 != FFA_SUCCESS_SMC32) {			\
			ERROR("ffa_partition_info_get failed!\n");		\
			return TEST_RESULT_FAIL;				\
		}								\
	} while (0)

#define GET_TFTF_MAILBOX(mb)							\
	do {									\
		if (!get_tftf_mailbox(&mb)) {					\
			ERROR("Mailbox not configured!\nThis test relies on"	\
			" test suite \"FF-A RXTX Mapping\" to map/configure"	\
			" RXTX buffers\n");					\
			return TEST_RESULT_FAIL;				\
		}								\
	} while (false);

#define CHECK_SPMC_TESTING_SETUP(ffa_major, ffa_minor, expected_uuids)		\
	do {									\
		SKIP_TEST_IF_AARCH32();						\
		const size_t expected_uuids_size =				\
			 sizeof(expected_uuids) / sizeof(struct ffa_uuid);	\
		test_result_t ret = check_spmc_testing_set_up(			\
			ffa_major, ffa_minor, expected_uuids, 			\
			expected_uuids_size);					\
		if (ret != TEST_RESULT_SUCCESS) {				\
			return ret;						\
		}								\
	} while (false);

#define SKIP_TEST_IF_TRBE_NOT_SUPPORTED()					\
	do {									\
		if (!get_armv9_0_trbe_support()) {				\
			tftf_testcase_printf("ARMv9-TRBE not supported\n");	\
			return TEST_RESULT_SKIPPED;				\
		}								\
	} while (false)

#define SKIP_TEST_IF_TRF_NOT_SUPPORTED()					\
	do {									\
		if (!get_armv8_4_trf_support()) {				\
			tftf_testcase_printf("ARMv8.4-TRF not supported\n");	\
			return TEST_RESULT_SKIPPED;				\
		}								\
	} while (false)

#define SKIP_TEST_IF_SYS_REG_TRACE_NOT_SUPPORTED()				\
	do {									\
		if (!get_armv8_0_sys_reg_trace_support()) {			\
			tftf_testcase_printf("ARMv8-system register"		\
					     "trace not supported\n");		\
			return TEST_RESULT_SKIPPED;				\
		}								\
	} while (false)

#define SKIP_TEST_IF_AFP_NOT_SUPPORTED()					\
	do {									\
		if (!get_feat_afp_present()) {					\
			tftf_testcase_printf("ARMv8.7-afp not supported");	\
			return TEST_RESULT_SKIPPED;				\
		}								\
	} while (false)

/* Helper macro to verify if system suspend API is supported */
#define is_psci_sys_susp_supported()	\
		(tftf_get_psci_feature_info(SMC_PSCI_SYSTEM_SUSPEND)		\
					== PSCI_E_SUCCESS)

/* Helper macro to verify if PSCI_STAT_COUNT API is supported */
#define is_psci_stat_count_supported()	\
		(tftf_get_psci_feature_info(SMC_PSCI_STAT_COUNT)		\
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

/*
 * Helper function to set TFTF global mailbox for SPM related tests.
 * This function should be invoked by the first TFTF test that requires
 * RX and/or TX buffers.
 */
void set_tftf_mailbox(const struct mailbox_buffers *mb);

/*
 * Helper function to get TFTF global mailbox for SPM related tests.
 * This function should be called by all tests that require access to RX or TX
 * buffers, after the function 'set_tftf_mailbox' has been used by the first
 * test to rely on RX and TX buffers.
 */
bool get_tftf_mailbox(struct mailbox_buffers *mb);

test_result_t check_spmc_testing_set_up(uint32_t ffa_version_major,
	uint32_t ffa_version_minor, const struct ffa_uuid *ffa_uuids,
	size_t ffa_uuids_size);

/**
 * Turn on all cpus to execute a test in all.
 * - 'cpu_on_handler' should have the code containing the test.
 * - 'cpu_booted' is used for notifying which cores the test has been executed.
 * This should be used in the test executed by cpu_on_handler at the end of
 * processing to make sure it complies with this function's implementation.
 */
test_result_t spm_run_multi_core_test(uintptr_t cpu_on_handler,
				      event_t *cpu_booted);

/**
 * Call FFA_RUN in the designated SP to make it reach the message loop.
 * Used within CPU_ON handlers, to bring up the SP in the current core.
 */
bool spm_core_sp_init(ffa_id_t sp_id);

/**
 * Enable/Disable managed exit interrupt for the provided SP.
 */
bool spm_set_managed_exit_int(ffa_id_t sp_id, bool enable);

#endif /* __TEST_HELPERS_H__ */
