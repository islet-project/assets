#
# Copyright (c) 2021, Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

TESTS_SOURCES	+=							\
	$(addprefix tftf/tests/runtime_services/realm_payload/,		\
		realm_payload_test_helpers.c				\
		realm_payload_test.c					\
	)
