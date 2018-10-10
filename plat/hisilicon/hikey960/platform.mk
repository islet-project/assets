#
# Copyright (c) 2018, Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

HIKEY960_PATH	:=	plat/hisilicon/hikey960

PLAT_INCLUDES	:=	-I${HIKEY960_PATH}/include/

PLAT_SOURCES	:=	${HIKEY960_PATH}/hikey960_setup.c		\
			${HIKEY960_PATH}/hikey960_pwr_state.c		\
			${HIKEY960_PATH}/aarch64/plat_helpers.S		\
			drivers/arm/pl011/${ARCH}/pl011_console.S	\
			drivers/arm/gic/gic_common.c			\
			drivers/arm/gic/gic_v2.c			\
			drivers/arm/gic/arm_gic_v2.c			\
			drivers/arm/timer/system_timer.c		\
			drivers/arm/timer/private_timer.c		\
			plat/arm/common/arm_timers.c

ifeq ($(USE_NVM),1)
$(error "Hikey960 port of TFTF doesn't currently support USE_NVM=1")
endif
ifneq ($(TESTS),tftf-validation)
$(error "Hikey960 port currently only supports tftf-validation")
endif
