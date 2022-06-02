#!/bin/bash

#
# Copyright (c) 2020-2022, Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

# Generate a JSON file which will be fed to TF-A as SPM_LAYOUT_FILE to package
# Secure Partitions as part of FIP.
# Note the script will append the partition to the existing layout file.
# If you wish to only generate a layout file with this partition first run
# "make realclean" to remove the existing file.

# $1 = Secure Partition
# $2 = Platform built path
# $3 = Ivy Shim present
# Output = $2/sp_layout.json

GENERATED_JSON=$2/sp_layout.json
primary_dts="$1.dts"

# Remove closing bracket and add comma if the dts is already present.
if [ ! -f "$GENERATED_JSON" ]; then
	echo -e "{\n" >> "$GENERATED_JSON"
else
	if [ "$1" == "ivy" ]; then
		if [ "$3" == "1" ]; then
			primary_dts="ivy-sel1.dts"
		else
			primary_dts="ivy-sel0.dts"
		fi
	fi

	if [ $(grep "$primary_dts" "$GENERATED_JSON" | wc -l) -eq "0" ]; then
		sed -i '$d' "$GENERATED_JSON"
		sed -i '$ s/$/,/' "$GENERATED_JSON"
		echo -e "\n" >> "$GENERATED_JSON"
	else
		exit 0
	fi
fi

# To demonstrate communication between SP's, two cactus S-EL1 instances used.
# To also test mapping of the RXTX region a third cactus S-EL1 instance is used.
# cactus-primary, cactus-secondary and cactus-tertiary have same binary but
# different partition manifests.
if [ "$1" == "cactus" ]; then
	echo -e "\t\"$1-primary\" : {\n \
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
	\t\"owner\": \"Plat\"\n\t}" \
	>> "$GENERATED_JSON"
elif [ "$1" == "ivy" ]; then
	echo -e "\t\"ivy\" : {\n \
	\t\"image\": \"ivy.bin\",\n \
	\t\"pm\": \"$primary_dts\",\n \
	\t\"owner\": \"Plat\"\n\t}" >> "$GENERATED_JSON"
else
	echo -e "\nWarning: Secure Partition not supported\n"
fi

echo -e "}" >> "$GENERATED_JSON"
