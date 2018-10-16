#
# Copyright (c) 2018, Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

TESTS_SOURCES	+=							\
	$(addprefix tftf/tests/runtime_services/standard_service/pmf/api_tests/runtime_instr/, \
		test_pmf_rt_instr.c					\
	)
