#
# Copyright (c) 2018, Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

TESTS_SOURCES	+=							\
	$(addprefix tftf/tests/runtime_services/standard_service/psci/system_tests/, \
		test_psci_hotplug_stress.c				\
		test_psci_on_off_suspend_stress.c			\
		test_psci_system_suspend_stress.c			\
	)
