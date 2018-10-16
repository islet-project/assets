#
# Copyright (c) 2018, Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

TESTS_SOURCES	+=						\
	$(addprefix tftf/tests/framework_validation_tests/,	\
		test_timer_framework.c				\
		test_validation_events.c			\
		test_validation_irq.c				\
		test_validation_nvm.c				\
		test_validation_sgi.c				\
	)
