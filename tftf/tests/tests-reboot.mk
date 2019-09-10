#
# Copyright (c) 2019, Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

TESTS_SOURCES	+=							\
	$(addprefix tftf/tests/runtime_services/standard_service/psci/api_tests/, \
		system_reset/test_system_reset.c			\
		mem_protect/test_mem_protect.c				\
		psci_stat/test_psci_stat.c				\
		reset2/reset2.c 					\
	)

ifeq (${USE_NVM},0)
$(error Reboot tests require USE_NVM=1 to persist test results across reboots)
endif

ifeq (${NEW_TEST_SESSION},1)
$(error Reboot tests require NEW_TEST_SESSION=0 to persist test results across reboots)
endif
