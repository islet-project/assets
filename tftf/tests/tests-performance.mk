#
# Copyright (c) 2018-2019, Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

TESTS_SOURCES	+=	$(addprefix tftf/tests/performance_tests/,	\
	smc_latencies.c							\
	test_psci_latencies.c						\
)
