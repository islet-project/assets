#
# Copyright (c) 2022, Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

N1SDP_MAX_PE_PER_CPU	:= 1

$(eval $(call add_define,TFTF_DEFINES,N1SDP_MAX_PE_PER_CPU))
$(eval $(call add_define,NS_BL1U_DEFINES,N1SDP_MAX_PE_PER_CPU))
$(eval $(call add_define,NS_BL2U_DEFINES,N1SDP_MAX_PE_PER_CPU))

PLAT_INCLUDES	+=	-Iplat/arm/n1sdp/include/

PLAT_SOURCES	:=	drivers/arm/gic/arm_gic_v2v3.c		\
			drivers/arm/gic/gic_v2.c		\
			drivers/arm/gic/gic_v3.c		\
			drivers/arm/sp805/sp805.c		\
			drivers/arm/timer/private_timer.c	\
			drivers/arm/timer/system_timer.c	\
			plat/arm/n1sdp/${ARCH}/plat_helpers.S	\
			plat/arm/n1sdp/plat_setup.c		\
			plat/arm/n1sdp/n1sdp_mem_prot.c		\
			plat/arm/n1sdp/n1sdp_pwr_state.c		\
			plat/arm/n1sdp/n1sdp_topology.c

CACTUS_SOURCES	+=	plat/arm/n1sdp/${ARCH}/plat_helpers.S
IVY_SOURCES	+=	plat/arm/n1sdp/${ARCH}/plat_helpers.S

PLAT_TESTS_SKIP_LIST	:=	plat/arm/n1sdp/tests_to_skip.txt

ifeq (${USE_NVM},1)
$(error "USE_NVM is not supported on N1SDP platforms")
endif

$(warning "TFTF on N1SDP is still in development and there may be issues")

include plat/arm/common/arm_common.mk
