#
# Copyright (c) 2022, Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

TESTS_SOURCES	+=	tftf/tests/misc_tests/test_invalid_access.c

TESTS_SOURCES	+=							\
	$(addprefix tftf/tests/runtime_services/realm_payload/,		\
		realm_payload_test_helpers.c				\
	)

TESTS_SOURCES	+=							\
	$(addprefix tftf/tests/runtime_services/secure_service/,	\
		ffa_helpers.c						\
		spm_common.c						\
		test_ffa_setup_and_discovery.c				\
)