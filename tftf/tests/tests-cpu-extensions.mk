#
# Copyright (c) 2018-2019, Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

TESTS_SOURCES	+=	$(addprefix tftf/tests/,			\
	extensions/amu/test_amu.c					\
	extensions/mte/test_mte.c					\
	extensions/sve/sve_operations.S					\
	extensions/sve/test_sve.c					\
	runtime_services/arm_arch_svc/smccc_arch_workaround_1.c		\
	runtime_services/arm_arch_svc/smccc_arch_workaround_2.c		\
	extensions/pauth/test_pauth.c					\
)
