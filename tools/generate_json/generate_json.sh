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

# $1 = Platform built path
# $2.. = List of Secure Partitions
# Output = $1/sp_layout.json

GENERATED_JSON=$1/sp_layout.json
shift # Shift arguments 1

PARTITION_ALREADY_PRESENT=false

CACTUS_PRESENT=false
IVY_PRESENT=false
IVY_SHIM_PRESENT=false

for target in "$@"
do
	case $target in
		cactus)
			CACTUS_PRESENT=true
			;;
		ivy)
			IVY_PRESENT=true
			;;
		ivy_shim)
			IVY_SHIM_PRESENT=true
			;;
		*)
			echo "Invalid target $target"
			exit 1
			;;
	esac
done

echo -e "{" > $GENERATED_JSON

# To demonstrate communication between SP's, two cactus S-EL1 instances used.
# To also test mapping of the RXTX region a third cactus S-EL1 instance is used.
# cactus-primary, cactus-secondary and cactus-tertiary have same binary but
# different partition manifests.
if [ $CACTUS_PRESENT == "true" ]; then
	echo -ne "\t\"cactus-primary\" : {\n \
	\t\"image\": {\n \
	\t\t\"file\": \"cactus.bin\",\n \
	\t\t\"offset\":\"0x2000\"\n\
	\t},\n \
	\t\"pm\": {\n \
	\t\t\"file\": \"cactus.dts\",\n \
	\t\t\"offset\":\"0x1000\"\n\
	\t},\n
	\t\"owner\": \"SiP\"\n\t},\n\n\t\"cactus-secondary\" : {\n \
	\t\"image\": \"cactus.bin\",\n \
	\t\"pm\": \"cactus-secondary.dts\",\n \
	\t\"owner\": \"Plat\"\n\t},\n\n\t\"cactus-tertiary\" : {\n \
	\t\"image\": \"cactus.bin\",\n \
	\t\"pm\": \"cactus-tertiary.dts\",\n \
	\t\"owner\": \"Plat\"\n\t}" \
	>> "$GENERATED_JSON"
	PARTITION_ALREADY_PRESENT=true
fi
if [ $IVY_PRESENT == "true" ]; then
	if [ $PARTITION_ALREADY_PRESENT == "true" ]; then
		echo -ne ",\n\n" >> $GENERATED_JSON
	fi
	echo -ne "\t\"ivy\" : {\n \
	\t\"image\": \"ivy.bin\",\n \
	\t\"pm\": \"ivy-sel0.dts\",\n \
	\t\"owner\": \"Plat\"\n\t}" >> "$GENERATED_JSON"
	PARTITION_ALREADY_PRESENT=true
elif [ $IVY_SHIM_PRESENT == "true" ]; then
	if [ $PARTITION_ALREADY_PRESENT == "true" ]; then
		echo -ne ",\n\n" >> $GENERATED_JSON
	fi
	echo -ne "\t\"ivy_shim\" : {\n \
	\t\"image\": \"ivy.bin\",\n \
	\t\"pm\": \"ivy-sel1.dts\",\n \
	\t\"owner\": \"Plat\"\n\t}" >> "$GENERATED_JSON"
	PARTITION_ALREADY_PRESENT=true
fi

echo -e "\n}" >> "$GENERATED_JSON"
