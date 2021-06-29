#
# Copyright (c) 2020, Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

# Default number of threads per CPU on FVP
TC0_MAX_PE_PER_CPU	:= 1

# Check the PE per core count
ifneq ($(TC0_MAX_PE_PER_CPU),$(filter $(TC0_MAX_PE_PER_CPU),1 2))
$(error "Incorrect TC0_MAX_PE_PER_CPU specified for TC0 port")
endif

# Pass FVP_MAX_PE_PER_CPU to the build system
$(eval $(call add_define,TFTF_DEFINES,TC0_MAX_PE_PER_CPU))
$(eval $(call add_define,NS_BL1U_DEFINES,TC0_MAX_PE_PER_CPU))
$(eval $(call add_define,NS_BL2U_DEFINES,TC0_MAX_PE_PER_CPU))

PLAT_INCLUDES	+=	-Iplat/arm/tc0/include/

PLAT_SOURCES	:=	drivers/arm/gic/arm_gic_v2v3.c		\
			drivers/arm/gic/gic_v2.c		\
			drivers/arm/gic/gic_v3.c		\
			drivers/arm/sp805/sp805.c		\
			drivers/arm/timer/private_timer.c	\
			drivers/arm/timer/system_timer.c	\
			plat/arm/tc0/${ARCH}/plat_helpers.S	\
			plat/arm/tc0/plat_setup.c		\
			plat/arm/tc0/tc0_mem_prot.c		\
			plat/arm/tc0/tc0_pwr_state.c		\
			plat/arm/tc0/tc0_topology.c

CACTUS_SOURCES	+=	plat/arm/tc0/${ARCH}/plat_helpers.S
IVY_SOURCES	+=	plat/arm/tc0/${ARCH}/plat_helpers.S

PLAT_TESTS_SKIP_LIST	:=	plat/arm/tc0/tests_to_skip.txt

ifeq (${USE_NVM},1)
$(error "USE_NVM is not supported on TC0 platforms")
endif

include plat/arm/common/arm_common.mk
