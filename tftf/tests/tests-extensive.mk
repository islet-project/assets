#
# Copyright (c) 2018, Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

# Run all standard tests, plus the extensive ones.
include tftf/tests/tests-standard.mk
TESTS_MAKEFILE += tftf/tests/tests-psci-extensive.mk

include ${TESTS_MAKEFILE}
