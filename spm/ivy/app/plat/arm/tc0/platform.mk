#
# Copyright (c) 2021-2022, Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

TC0_IVY_BASE		= spm/ivy/app/plat/arm/tc0

PLAT_INCLUDES		+= -I${TC0_IVY_BASE}/include/

# Add the FDT source
ifeq ($(IVY_SHIM),0)
IVY_DTS		= ${TC0_IVY_BASE}/fdts/ivy-sel0.dts
else
IVY_DTS		= ${TC0_IVY_BASE}/fdts/ivy-sel1.dts
endif

# List of FDTS to copy
FDTS_CP_LIST		=  $(IVY_DTS)
