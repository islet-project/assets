#
# Copyright (c) 2018-2022, Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

TESTS_SOURCES	+=								\
	$(addprefix tftf/tests/runtime_services/trusted_os/tsp/,		\
		test_irq_preempted_std_smc.c					\
		test_irq_spurious_gicv2.c					\
		test_smc_tsp_std_fn_call.c					\
		test_tsp_fast_smc.c						\
		test_normal_int_switch.c					\
		test_pstate_after_exception.c					\
	)
