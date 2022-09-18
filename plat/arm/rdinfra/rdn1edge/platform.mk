#
# Copyright (c) 2019-2022, Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

include plat/arm/sgi/common/sgi_common.mk

PLAT_INCLUDES		+=	-Iplat/arm/rdinfra/rdn1edge/include/

PLAT_SOURCES		+=	plat/arm/rdinfra/rdn1edge/topology.c

PLAT_TESTS_SKIP_LIST	:=	plat/arm/rdinfra/rdn1edge/tests_to_skip.txt

ifdef CSS_SGI_PLATFORM_VARIANT
$(error "CSS_SGI_PLATFORM_VARIANT should not be set for RD-N1-Edge, \
    currently set to ${CSS_SGI_PLATFORM_VARIANT}.")
endif
