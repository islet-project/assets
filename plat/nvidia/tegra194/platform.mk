#
# Copyright (c) 2020, NVIDIA Corporation. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

PLAT_INCLUDES		:=	-Iplat/nvidia/tegra194/include/

PLAT_SOURCES		:=	drivers/arm/gic/arm_gic_v2.c			\
				drivers/arm/gic/gic_common.c			\
				drivers/arm/gic/gic_v2.c			\
				drivers/arm/timer/private_timer.c		\
				drivers/ti/uart/aarch64/16550_console.S		\
				plat/nvidia/tegra194/helpers.S			\
				plat/nvidia/tegra194/pwr_state.c		\
				plat/nvidia/tegra194/pwr_mgmt.c			\
				plat/nvidia/tegra194/setup.c			\
				plat/nvidia/tegra194/topology.c			\
				plat/nvidia/tegra194/timers.c

PLAT_TESTS_SKIP_LIST	:=	plat/nvidia/tegra194/tests_to_skip.txt

TFTF_CFLAGS		+=	-Wno-maybe-uninitialized

ENABLE_ASSERTIONS	:=	1

ifeq ($(USE_NVM),1)
$(error "Tegra194 port of TFTF doesn't currently support USE_NVM=1")
endif
