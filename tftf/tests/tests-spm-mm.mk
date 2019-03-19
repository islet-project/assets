#
# Copyright (c) 2018, Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

TESTS_SOURCES	+=							\
	$(addprefix tftf/tests/runtime_services/mm_service/,		\
		secure_service_helpers.c				\
		test_secure_service_handle.c 				\
		test_secure_service_interrupts.c			\
	)
