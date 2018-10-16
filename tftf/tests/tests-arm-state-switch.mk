#
# Copyright (c) 2018, Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

TESTS_SOURCES	+=							\
	$(addprefix tftf/tests/runtime_services/sip_service/,		\
		test_exec_state_switch.c				\
		test_exec_state_switch_asm.S				\
	)
