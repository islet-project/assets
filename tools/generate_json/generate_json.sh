#!/bin/bash

#
# Copyright (c) 2020, Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

# Generate a JSON file which will be fed to TF-A as SPM_LAYOUT_FILE to package
# Secure Partitions as part of FIP.

# $1 = Secure Partition (cactus)
# $2 = Platform (fvp)
# $3 = Build Type
# Output = build/plat/<Build Type>/sp_layout.json

GENERATED_JSON=build/$2/$3/sp_layout.json

# To demonstrate communication between SP's, two cactus S-EL1 instances used.
# cactus-primary and cactus-secondary has same binary but different
# partition manifest.
if [ "$1" == "cactus" ]; then
	echo -e "{\n\t\"$1-primary\" : {\n \
	\t\"image\": \"$1.bin\",\n \
	\t\"pm\": \"../../../spm/$1/$1.dts\"\n\t},\n\n\t\"$1-secondary\" : {\n \
	\t\"image\": \"$1.bin\",\n \
	\t\"pm\": \"../../../spm/$1/$1-secondary.dts\" \n \
	}\n}" \
	> "$GENERATED_JSON"
else
	echo -e "\nWarning: Only Cactus is supported as Secure Partition\n"
fi
