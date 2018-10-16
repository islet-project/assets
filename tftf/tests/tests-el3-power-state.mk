#
# Copyright (c) 2018, Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

TESTS_SOURCES	+=							\
	$(addprefix tftf/tests/runtime_services/standard_service/psci/api_tests/validate_power_state/, \
		test_validate_power_state.c 				\
	)
