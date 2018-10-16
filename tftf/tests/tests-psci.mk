#
# Copyright (c) 2018, Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

TESTS_SOURCES	+=								\
	$(addprefix tftf/tests/runtime_services/standard_service/,		\
		query_std_svc.c 						\
		unknown_smc.c 							\
	)

TESTS_SOURCES	+=						\
	$(addprefix tftf/tests/runtime_services/standard_service/psci/api_tests/, \
		affinity_info/test_psci_affinity_info.c		\
		cpu_hotplug/test_psci_hotplug.c			\
		cpu_hotplug/test_psci_hotplug_invalid.c		\
		cpu_suspend/test_suspend.c			\
		mem_protect_check/mem_protect_check.c		\
		migrate_info_type/test_migrate_info_type.c	\
		psci_features/test_psci_features.c		\
		psci_node_hw_state/test_node_hw_state.c		\
		psci_stat/test_psci_stat.c			\
		psci_version/test_psci_version.c		\
		system_suspend/test_psci_system_suspend.c	\
	)
