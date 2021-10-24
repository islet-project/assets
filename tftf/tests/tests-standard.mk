#
# Copyright (c) 2018-2021, Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

TESTS_MAKEFILE := $(addprefix tftf/tests/,	\
	tests-arm-state-switch.mk		\
	tests-boot-req.mk			\
	tests-cpu-extensions.mk			\
	tests-el3-power-state.mk		\
	tests-performance.mk			\
	tests-pmu-leakage.mk			\
	tests-psci.mk				\
	tests-runtime-instrumentation.mk	\
	tests-sdei.mk				\
	tests-single-fault.mk			\
	tests-smc.mk 				\
	tests-spm.mk				\
	tests-template.mk			\
	tests-tftf-validation.mk		\
	tests-trng.mk				\
	tests-tsp.mk				\
	tests-uncontainable.mk			\
	tests-debugfs.mk                        \
	tests-realm-payload.mk			\
)

include ${TESTS_MAKEFILE}
