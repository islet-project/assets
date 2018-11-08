#
# Copyright (c) 2018, Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

TESTS_SOURCES	+=							\
	$(addprefix tftf/tests/runtime_services/secure_service/,	\
		spci_helpers.c						\
		test_spci_handle_open.c					\
		test_spci_blocking_request.c				\
		test_spci_non_blocking_request.c			\
		test_spci_blocking_while_busy.c				\
	)
