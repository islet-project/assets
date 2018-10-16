#
# Copyright (c) 2018, Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

TESTS_SOURCES	+=	$(addprefix tftf/tests/fwu_tests/,	\
	test_fwu_auth.c						\
	test_fwu_toc.c						\
)

TESTS_SOURCES	+=	plat/common/fwu_nvm_accessors.c
