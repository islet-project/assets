#
# Copyright (c) 2018, Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

TESTS_SOURCES	+=							\
	$(addprefix tftf/tests/runtime_services/standard_service/psci/api_tests/, \
		mem_protect/test_mem_protect.c				\
		psci_stat/test_psci_stat.c				\
		reset2/reset2.c 					\
		system_off/test_system_off.c 				\
	)
