#
# Copyright (c) 2018-2021, Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

TESTS_SOURCES	+=	$(addprefix tftf/tests/,			\
	extensions/amu/test_amu.c					\
	extensions/ecv/test_ecv.c					\
	extensions/fgt/test_fgt.c					\
	extensions/mte/test_mte.c					\
	extensions/pauth/test_pauth.c					\
	extensions/sve/sve_operations.S					\
	extensions/sve/test_sve.c					\
	extensions/trbe/test_trbe.c					\
	runtime_services/arm_arch_svc/smccc_arch_soc_id.c		\
	runtime_services/arm_arch_svc/smccc_arch_workaround_1.c		\
	runtime_services/arm_arch_svc/smccc_arch_workaround_2.c		\
)
