#
# Copyright (c) 2018-2021, Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

TESTS_SOURCES	+=							\
	$(addprefix tftf/tests/runtime_services/secure_service/,	\
		ffa_helpers.c						\
		spm_common.c						\
		test_ffa_direct_messaging.c				\
		test_ffa_interrupts.c					\
		test_ffa_secure_interrupts.c				\
		test_ffa_memory_sharing.c				\
		test_ffa_setup_and_discovery.c				\
		test_ffa_notifications.c				\
		test_spm_cpu_features.c					\
		test_spm_smmu.c						\
		test_ffa_exceptions.c					\
	)

TESTS_SOURCES	+=							\
	$(addprefix tftf/tests/runtime_services/realm_payload/,		\
		realm_payload_test_helpers.c				\
	)
