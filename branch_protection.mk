#
# Copyright (c) 2020, Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

# Default, static values for build variables, listed in alphabetic order.
# Dependencies between build options, if any, are handled in the top-level
# Makefile, after this file is included. This ensures that the former is better
# poised to handle dependencies, as all build variables would have a default
# value by then.

# Select the branch protection features to use.
BRANCH_PROTECTION	:= 0

# Flag to enable Branch Target Identification in the TFTF.
# Internal flag not meant for direct setting.
# Use BRANCH_PROTECTION to enable BTI.
ENABLE_BTI		:= 0

# Enable Pointer Authentication support in the TFTF.
# Internal flag not meant for direct setting.
# Use BRANCH_PROTECTION to enable PAUTH.
ENABLE_PAUTH		:= 0

# Process BRANCH_PROTECTION value and set
# Pointer Authentication and Branch Target Identification flags
ifeq (${BRANCH_PROTECTION},0)
	# Default value turns off all types of branch protection
	BP_OPTION := none
else ifneq (${ARCH},aarch64)
        $(error BRANCH_PROTECTION requires AArch64)
else ifeq (${BRANCH_PROTECTION},1)
	# Enables all types of branch protection features
	BP_OPTION := standard
	ENABLE_BTI := 1
	ENABLE_PAUTH := 1
else ifeq (${BRANCH_PROTECTION},2)
	# Return address signing to its standard level
	BP_OPTION := pac-ret
	ENABLE_PAUTH := 1
else ifeq (${BRANCH_PROTECTION},3)
	# Extend the signing to include leaf functions
	BP_OPTION := pac-ret+leaf
	ENABLE_PAUTH := 1
else ifeq (${BRANCH_PROTECTION},4)
	# Turn on branch target identification mechanism
	BP_OPTION := bti
	ENABLE_BTI := 1
else
        $(error Unknown BRANCH_PROTECTION value ${BRANCH_PROTECTION})
endif
