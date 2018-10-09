#
# Copyright (c) 2018, Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

# Path to the XML file listing the tests to run. If there is a platform-specific
# test file, use it. If not, use the common one. If the user specified another
# one, use that one instead.
ifneq ($(wildcard ${PLAT_PATH}/tests.xml),)
	TESTS_FILE := ${PLAT_PATH}/tests.xml
else
	TESTS_FILE := tftf/tests/tests-common.xml
endif

# Check that the selected tests file exists.
ifeq (,$(wildcard ${TESTS_FILE}))
  $(error "The file TESTS_FILE points to cannot be found")
endif

SPM_TESTS_SOURCES	:=						\
	$(addprefix tftf/tests/runtime_services/secure_service/,	\
		secure_service_helpers.c				\
		test_secure_service_handle.c 				\
		test_secure_service_interrupts.c			\
	)

FWU_TESTS_SOURCES	:=						\
	tftf/tests/fwu_tests/test_fwu_toc.c				\
	tftf/tests/fwu_tests/test_fwu_auth.c				\
	plat/common/fwu_nvm_accessors.c

TESTS_SOURCES	:=	$(addprefix tftf/tests/,			\
	extensions/amu/test_amu.c					\
	framework_validation_tests/test_timer_framework.c		\
	framework_validation_tests/test_validation_events.c	\
	framework_validation_tests/test_validation_irq.c		\
	framework_validation_tests/test_validation_nvm.c		\
	framework_validation_tests/test_validation_sgi.c		\
	misc_tests/inject_serror.S \
	misc_tests/test_single_fault.c \
	misc_tests/test_uncontainable.c \
	performance_tests/smc_latencies.c				\
	misc_tests/boot_req_tests/test_cntfrq.c			\
	runtime_services/arm_arch_svc/smccc_arch_workaround_1.c		\
	runtime_services/arm_arch_svc/smccc_arch_workaround_2.c		\
	runtime_services/sip_service/test_exec_state_switch.c	\
	runtime_services/sip_service/test_exec_state_switch_asm.S \
	runtime_services/standard_service/pmf/api_tests/runtime_instr/test_pmf_rt_instr.c	\
	runtime_services/standard_service/psci/api_tests/affinity_info/test_psci_affinity_info.c	\
	runtime_services/standard_service/psci/api_tests/cpu_hotplug/test_psci_hotplug.c	\
	runtime_services/standard_service/psci/api_tests/cpu_hotplug/test_psci_hotplug_invalid.c \
	runtime_services/standard_service/psci/api_tests/cpu_suspend/test_suspend.c	\
	runtime_services/standard_service/psci/api_tests/migrate_info_type/test_migrate_info_type.c \
	runtime_services/standard_service/psci/api_tests/psci_features/test_psci_features.c \
	runtime_services/standard_service/psci/api_tests/psci_node_hw_state/test_node_hw_state.c \
	runtime_services/standard_service/psci/api_tests/psci_stat/test_psci_stat.c	\
	runtime_services/standard_service/psci/api_tests/psci_version/test_psci_version.c	\
	runtime_services/standard_service/psci/api_tests/system_off/test_system_off.c \
	runtime_services/standard_service/psci/api_tests/system_suspend/test_psci_system_suspend.c \
	runtime_services/standard_service/psci/api_tests/validate_power_state/test_validate_power_state.c \
	runtime_services/standard_service/psci/system_tests/test_psci_hotplug_stress.c		\
	runtime_services/standard_service/psci/system_tests/test_psci_on_off_suspend_stress.c		\
	runtime_services/standard_service/psci/system_tests/test_psci_system_suspend_stress.c	\
	runtime_services/standard_service/psci/api_tests/mem_protect/test_mem_protect.c	\
	runtime_services/standard_service/psci/api_tests/mem_protect_check/mem_protect_check.c \
	runtime_services/standard_service/psci/api_tests/reset2/reset2.c \
	runtime_services/standard_service/query_std_svc.c \
	runtime_services/standard_service/unknown_smc.c \
	runtime_services/standard_service/sdei/system_tests/sdei_entrypoint.S \
	runtime_services/standard_service/sdei/system_tests/test_sdei.c \
	runtime_services/standard_service/sdei/system_tests/test_sdei_state.c \
	runtime_services/trusted_os/tsp/test_irq_preempted_std_smc.c \
	runtime_services/trusted_os/tsp/test_normal_int_switch.c \
	runtime_services/trusted_os/tsp/test_smc_tsp_std_fn_call.c \
	runtime_services/trusted_os/tsp/test_tsp_fast_smc.c \
)

TESTS_SOURCES	+=	${SPM_TESTS_SOURCES} \
			${FWU_TESTS_SOURCES}

# The following source files are part of the "template" testsuite, which aims
# at providing template test code as a starting point for developing new tests.
# They don't do anything useful in terms of testing so they are disabled by
# default. Uncomment those lines along with the corresponding test suite XML
# node in the tests/tests.xml file to enable them.
# TESTS_SOURCES	+=	tftf/tests/template_tests/test_template_single_core.c		\
#			tftf/tests/template_tests/test_template_multi_core.c
