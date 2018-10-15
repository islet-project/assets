#
# Copyright (c) 2018, Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

PLAT_INCLUDES	:=	-Iplat/arm/board/fvp/include/

PLAT_SOURCES	:=	drivers/arm/gic/arm_gic_v2v3.c			\
			drivers/arm/gic/gic_v2.c			\
			drivers/arm/gic/gic_v3.c			\
			drivers/arm/sp805/sp805.c			\
			drivers/arm/timer/private_timer.c		\
			drivers/arm/timer/system_timer.c		\
			lib/semihosting/${ARCH}/semihosting_call.S	\
			lib/semihosting/semihosting.c			\
			plat/arm/board/fvp/${ARCH}/plat_helpers.S	\
			plat/arm/board/fvp/fvp_pwr_state.c		\
			plat/arm/board/fvp/fvp_topology.c		\
			plat/arm/board/fvp/fvp_mem_prot.c		\
			plat/arm/board/fvp/plat_setup.c

CACTUS_SOURCES	+=	plat/arm/board/fvp/${ARCH}/plat_helpers.S

# Firmware update is implemented on FVP.
FIRMWARE_UPDATE := 1

include plat/arm/common/arm_common.mk
