/*
 * Copyright (c) 2017-2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef CACTUS_TESTS_H
#define CACTUS_TESTS_H

/*
 * Test functions
 */

/*
 * Test other things like the version number returned by SPM.
 */
void misc_tests(void);

/*
 * The Arm TF is responsible for setting up system registers on behalf of the
 * Secure Partition. For example, TF is supposed to allow Secure Partitions to
 * perform cache maintenance operations (by setting the SCTLR_EL1.UCI bit).
 *
 * This function attempts to verify that we indeed have access to these system
 * features from S-EL0. These tests report their results on the UART. They do
 * not recover from a failure : when an error is encountered they will most
 * likely trigger an exception into S-EL1.
 */
void system_setup_tests(void);

/*
 * Exercise the SP_MEMORY_ATTRIBUTES_SET_AARCH64 SMC interface. A variety of
 * valid and invalid requests to change memory attributes are tested.
 *
 * These tests report their results on the UART. They do not recover from a
 * failure : when an error is encountered they endlessly loop.
 */
void mem_attr_changes_tests(void);

#endif /* CACTUS_TESTS_H */
