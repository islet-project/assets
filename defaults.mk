#
# Copyright (c) 2018, Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

# Default, static values for build variables, listed in alphabetic order.
# Dependencies between build options, if any, are handled in the top-level
# Makefile, after this file is included. This ensures that the former is better
# poised to handle dependencies, as all build variables would have a default
# value by then.

# The Target build architecture. Supported values are: aarch64, aarch32.
ARCH			:= aarch64

# ARM Architecture major and minor versions: 8.0 by default.
ARM_ARCH_MAJOR		:= 8
ARM_ARCH_MINOR		:= 0

# Base commit to perform code check on
BASE_COMMIT		:= origin/master

# Debug/Release build
DEBUG			:= 0

# Build platform
DEFAULT_PLAT		:= fvp

# Whether the Firmware Update images (i.e. NS_BL1U and NS_BL2U images) should be
# built. The platform makefile is free to override this value.
FIRMWARE_UPDATE		:= 0

# Enable FWU helper functions and inline tests in NS_BL1U and NS_BL2U images.
FWU_BL_TEST := 1

# Whether a new test session should be started every time or whether the
# framework should try to resume a previous one if it was interrupted
NEW_TEST_SESSION	:= 1

# Use non volatile memory for storing results
USE_NVM			:= 0

# Build verbosity
V			:= 0
