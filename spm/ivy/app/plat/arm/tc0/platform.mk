#
# Copyright (c) 2021, Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

TC0_IVY_BASE		= spm/ivy/app/plat/arm/tc0

PLAT_INCLUDES		+= -I${TC0_IVY_BASE}/include/

# Add the FDT source
IVY_DTS		= ${TC0_IVY_BASE}/fdts/ivy.dts

# List of FDTS to copy
FDTS_CP_LIST		= ${TC0_IVY_BASE}/fdts/ivy.dts
