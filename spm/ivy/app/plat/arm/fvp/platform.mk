#
# Copyright (c) 2021, Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

FVP_IVY_BASE		= spm/ivy/app/plat/arm/fvp

PLAT_INCLUDES		+= -I${FVP_IVY_BASE}/include/

# Add the FDT source
IVY_DTS		= ${FVP_IVY_BASE}/fdts/ivy.dts

# List of FDTS to copy
FDTS_CP_LIST		= ${FVP_IVY_BASE}/fdts/ivy.dts
