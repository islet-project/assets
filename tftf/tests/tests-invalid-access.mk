#
# Copyright (c) 2022, Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

TFTF_INCLUDES +=							\
	-Iinclude/runtime_services/host_realm_managment

TESTS_SOURCES	+=	tftf/tests/misc_tests/test_invalid_access.c

TESTS_SOURCES	+=							\
	$(addprefix tftf/tests/runtime_services/host_realm_managment/,	\
		host_realm_rmi.c					\
	)

TESTS_SOURCES	+=							\
	$(addprefix tftf/tests/runtime_services/secure_service/,	\
		${ARCH}/ffa_arch_helpers.S				\
		ffa_helpers.c						\
		spm_common.c						\
		test_ffa_setup_and_discovery.c				\
)