#
# Copyright (c) 2020-2021, Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

TESTS_SOURCES	+=								\
	$(addprefix tftf/tests/runtime_services/standard_service/sdei/system_tests/, \
		sdei_entrypoint.S 						\
		test_sdei.c 							\
		test_sdei_state.c 						\
		test_sdei_rm_any.c 						\
		test_sdei_pstate.c						\
	)
