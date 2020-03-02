#
# Copyright (c) 2018-2019, Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

# Default number of threads per CPU on FVP
FVP_MAX_PE_PER_CPU	:= 1

# Check the PE per core count
ifneq ($(FVP_MAX_PE_PER_CPU),$(filter $(FVP_MAX_PE_PER_CPU),1 2))
$(error "Incorrect FVP_MAX_PE_PER_CPU specified for FVP port")
endif

# Pass FVP_MAX_PE_PER_CPU to the build system
$(eval $(call add_define,TFTF_DEFINES,FVP_MAX_PE_PER_CPU))
$(eval $(call add_define,NS_BL1U_DEFINES,FVP_MAX_PE_PER_CPU))
$(eval $(call add_define,NS_BL2U_DEFINES,FVP_MAX_PE_PER_CPU))

PLAT_INCLUDES	:=	-Iplat/arm/fvp/include/

PLAT_SOURCES	:=	drivers/arm/gic/arm_gic_v2v3.c			\
			drivers/arm/gic/gic_v2.c			\
			drivers/arm/gic/gic_v3.c			\
			drivers/arm/sp805/sp805.c			\
			drivers/arm/timer/private_timer.c		\
			drivers/arm/timer/system_timer.c		\
			plat/arm/fvp/${ARCH}/plat_helpers.S		\
			plat/arm/fvp/fvp_pwr_state.c			\
			plat/arm/fvp/fvp_topology.c			\
			plat/arm/fvp/fvp_mem_prot.c			\
			plat/arm/fvp/plat_setup.c

CACTUS_SOURCES	+=	plat/arm/fvp/${ARCH}/plat_helpers.S

# Firmware update is implemented on FVP.
FIRMWARE_UPDATE := 1

PLAT_TESTS_SKIP_LIST	:=	plat/arm/fvp/fvp_tests_to_skip.txt

include plat/arm/common/arm_common.mk
