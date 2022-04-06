#
# Copyright (c) 2022, Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

PLAT_INCLUDES	:=	-Iplat/arm/corstone1000/include/

PLAT_SOURCES	:=	drivers/arm/gic/arm_gic_v2.c			\
			drivers/arm/gic/gic_v2.c			\
			drivers/arm/timer/private_timer.c		\
			drivers/arm/timer/system_timer.c		\
			plat/arm/corstone1000/plat_helpers.S		\
			plat/arm/corstone1000/corstone1000_pwr_state.c	\
			plat/arm/corstone1000/corstone1000_topology.c	\
			plat/arm/corstone1000/corstone1000_mem_prot.c	\
			plat/arm/corstone1000/plat_setup.c

PLAT_SUPPORTS_NS_RESET	:=	1

# Process PLAT_SUPPORTS_NS_RESET flag
$(eval $(call assert_boolean,PLAT_SUPPORTS_NS_RESET))
$(eval $(call add_define,TFTF_DEFINES,PLAT_SUPPORTS_NS_RESET))

FIRMWARE_UPDATE := 0
PLAT_TESTS_SKIP_LIST    :=      plat/arm/corstone1000/tests_to_skip.txt

include plat/arm/common/arm_common.mk
