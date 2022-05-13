#
# Copyright (c) 2018-2022, Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

TESTS_SOURCES	+=	$(addprefix tftf/tests/,			\
	extensions/afp/test_afp.c					\
	extensions/amu/test_amu.c					\
	extensions/brbe/test_brbe.c					\
	extensions/ecv/test_ecv.c					\
	extensions/fgt/test_fgt.c					\
	extensions/mte/test_mte.c					\
	extensions/pauth/test_pauth.c					\
	extensions/sve/sve_operations.S					\
	extensions/sme/test_sme.c					\
	extensions/spe/test_spe.c                                       \
	extensions/sve/test_sve.c					\
	extensions/sys_reg_trace/test_sys_reg_trace.c			\
	extensions/trbe/test_trbe.c					\
	extensions/trf/test_trf.c					\
	extensions/wfxt/test_wfxt.c					\
	runtime_services/arm_arch_svc/smccc_arch_soc_id.c		\
	runtime_services/arm_arch_svc/smccc_arch_workaround_1.c		\
	runtime_services/arm_arch_svc/smccc_arch_workaround_2.c		\
	runtime_services/arm_arch_svc/smccc_arch_workaround_3.c		\
)
