#
# Copyright (c) 2018, Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

TESTS ?= standard

tests_files	:= $(wildcard tftf/tests/*.xml)
tests_sets	:= $(patsubst tftf/tests/tests-%.xml,%,${tests_files})
tests_sets	:= $(sort ${tests_sets})

PHONY: help_tests
help_tests:
	@echo "Supported Tests:"
	@$(foreach t, ${tests_sets}, printf "   %s\n" ${t};)

TESTS_FILE	:= tftf/tests/tests-${TESTS}.xml
TESTS_MAKEFILE	:= tftf/tests/tests-${TESTS}.mk

# Check that the selected tests file and makefile exist.
ifeq (,$(wildcard ${TESTS_FILE}))
  $(error "Tests file tftf/tests/tests-${TESTS}.xml does not exist.")
endif

ifeq (,$(wildcard ${TESTS_MAKEFILE}))
  $(error "Tests makefile tftf/tests/tests-${TESTS}.mk does not exist.")
endif

# Initialize variable before including all sub-makefiles that will append source
# files to it.
TESTS_SOURCES   :=

include ${TESTS_MAKEFILE}

# Some source files might be included by several test makefiles.
# Remove duplicate ones.
TESTS_SOURCES := $(sort ${TESTS_SOURCES})
