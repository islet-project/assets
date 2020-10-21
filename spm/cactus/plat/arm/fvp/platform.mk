#
# Copyright (c) 2020, Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

FVP_CACTUS_BASE		= spm/cactus/plat/arm/fvp

PLAT_INCLUDES		+= -I${FVP_CACTUS_BASE}/include/

# Add the FDT source
CACTUS_DTS		= ${FVP_CACTUS_BASE}/fdts/cactus.dts

# List of FDTS to copy
FDTS_CP_LIST		= ${FVP_CACTUS_BASE}/fdts/cactus.dts
FDTS_CP_LIST		+= ${FVP_CACTUS_BASE}/fdts/cactus-secondary.dts
FDTS_CP_LIST		+= ${FVP_CACTUS_BASE}/fdts/cactus-tertiary.dts
