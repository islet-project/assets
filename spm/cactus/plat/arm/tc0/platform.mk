#
# Copyright (c) 2020, Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

TC0_CACTUS_BASE		= spm/cactus/plat/arm/tc0

PLAT_INCLUDES		+= -I${TC0_CACTUS_BASE}/include/

# Add the FDT source
CACTUS_DTS		= ${TC0_CACTUS_BASE}/fdts/cactus.dts

# List of FDTS to copy
FDTS_CP_LIST		= ${TC0_CACTUS_BASE}/fdts/cactus.dts
FDTS_CP_LIST		+= ${TC0_CACTUS_BASE}/fdts/cactus-secondary.dts
FDTS_CP_LIST		+= ${TC0_CACTUS_BASE}/fdts/cactus-tertiary.dts
