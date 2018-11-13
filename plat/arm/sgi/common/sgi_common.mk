#
# Copyright (c) 2018, Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

PLAT_INCLUDES	:=	-Iplat/arm/sgi/common/include/

PLAT_SOURCES	:=	drivers/arm/gic/arm_gic_v2v3.c			\
			drivers/arm/gic/gic_v2.c			\
			drivers/arm/gic/gic_v3.c			\
			drivers/arm/sp805/sp805.c			\
			drivers/arm/timer/private_timer.c		\
			drivers/arm/timer/system_timer.c		\
			plat/arm/sgi/common/${ARCH}/plat_helpers.S	\
			plat/arm/sgi/common/plat_setup.c		\
			plat/arm/sgi/common/sgi_mem_prot.c		\
			plat/arm/sgi/common/sgi_pwr_state.c

include plat/arm/common/arm_common.mk

ifeq (${USE_NVM},1)
$(error "USE_NVM is not supported on SGI platforms")
endif
