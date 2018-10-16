#
# Copyright (c) 2018, Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

# Path to the XML file listing the tests to run. If there is a platform-specific
# test file, use it. If not, use the common one. If the user specified another
# one, use that one instead.
ifneq ($(wildcard ${PLAT_PATH}/tests.xml),)
	TESTS_FILE := ${PLAT_PATH}/tests.xml
else
	TESTS_FILE := tftf/tests/tests-common.xml
endif

# Check that the selected tests file exists.
ifeq (,$(wildcard ${TESTS_FILE}))
  $(error "The file TESTS_FILE points to cannot be found")
endif

# Initialize variable before including all sub-makefiles that will append source
# files to it.
TESTS_SOURCES   :=

include tftf/tests/tests-arm-state-switch.mk
include tftf/tests/tests-boot-req.mk
include tftf/tests/tests-cpu-extensions.mk
include tftf/tests/tests-el3-power-state.mk
include tftf/tests/tests-fwu.mk
include tftf/tests/tests-manual.mk
include tftf/tests/tests-performance.mk
include tftf/tests/tests-psci-extensive.mk
include tftf/tests/tests-psci.mk
include tftf/tests/tests-runtime-instrumentation.mk
include tftf/tests/tests-sdei.mk
include tftf/tests/tests-single-fault.mk
include tftf/tests/tests-spm.mk
include tftf/tests/tests-template.mk
include tftf/tests/tests-tftf-validation.mk
include tftf/tests/tests-tsp.mk
include tftf/tests/tests-uncontainable.mk

# Some source files might be included by several test makefiles.
# Remove duplicate ones.
TESTS_SOURCES := $(sort ${TESTS_SOURCES})
