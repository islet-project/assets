#!/bin/bash

#
# Copyright (c) 2020-2021, Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

# Generate a JSON file which will be fed to TF-A as SPM_LAYOUT_FILE to package
# Secure Partitions as part of FIP.

# $1 = Secure Partition (cactus)
# $2 = Platform built path
# Output = $2/sp_layout.json

GENERATED_JSON=$2/sp_layout.json

# To demonstrate communication between SP's, two cactus S-EL1 instances used.
# To also test mapping of the RXTX region a third cactus S-EL1 instance is used.
# cactus-primary, cactus-secondary and cactus-tertiary have same binary but
# different partition manifests.
if [ "$1" == "cactus" ]; then
	echo -e "{\n\t\"$1-primary\" : {\n \
	\t\"image\": {\n \
	\t\t\"file\": \"$1.bin\",\n \
	\t\t\"offset\":\"0x2000\"\n\
	\t},\n \
	\t\"pm\": {\n \
	\t\t\"file\": \"$1.dts\",\n \
	\t\t\"offset\":\"0x1000\"\n\
	\t},\n
	\t\"owner\": \"SiP\"\n\t},\n\n\t\"$1-secondary\" : {\n \
	\t\"image\": \"$1.bin\",\n \
	\t\"pm\": \"$1-secondary.dts\",\n \
	\t\"owner\": \"Plat\"\n\t},\n\n\t\"$1-tertiary\" : {\n \
	\t\"image\": \"$1.bin\",\n \
	\t\"pm\": \"$1-tertiary.dts\",\n \
	\t\"owner\": \"Plat\"\n\t},\n\n\t\"ivy\" : {\n \
	\t\"image\": \"ivy.bin\",\n \
	\t\"pm\": \"ivy.dts\", \n \
	\t\"owner\": \"Plat\"\n \
	}\n}" \
	> "$GENERATED_JSON"
else
	echo -e "\nWarning: Only Cactus is supported as Secure Partition\n"
fi
