#!/bin/bash

#
# Copyright (c) 2018-2020, Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

# Generate a DTB file from a base DTS file and the MAP file generated during the
# compilation of a Secure Partition.

# $1 = image_name (lowercase)
# $2 = path/to/file.dts
# $3 = build/$PLAT/$BUILD_TYPE/

ORIGINAL_DTS=$2
MAPFILE="$3/$1/$1.map"
EXTRA_DTS="$3/$1/$1_extra.dts"
COMBINED_DTS="$3/$1/$1_combined.dts"
PREPROCESSED_DTS="$3/$1/$1_preprocessed.dts"
GENERATED_DTB="$3/$1.dtb"

# Look for the start and end of the sections that are only known in the elf file
# after compiling the partition.

TEXT_START=$(grep __TEXT_START__ $MAPFILE | awk {'print $1'})
TEXT_END=$(grep __TEXT_END__ $MAPFILE | awk {'print $1'})

RODATA_START=$(grep __RODATA_START__ $MAPFILE | awk {'print $1'})
RODATA_END=$(grep __RODATA_END__ $MAPFILE | awk {'print $1'})

DATA_START=$(grep __DATA_START__ $MAPFILE | awk {'print $1'})
DATA_END=$(grep __DATA_END__ $MAPFILE | awk {'print $1'})

BSS_START=$(grep __BSS_START__ $MAPFILE | awk {'print $1'})
BSS_END=$(grep __BSS_END__ $MAPFILE | awk {'print $1'})

# Inject new sections to the base DTS

# Memory region generation discarded

cat "$ORIGINAL_DTS" > "$COMBINED_DTS"

INCLUDES="-I spm/cactus
	  -I spm/ivy
	  -I spm/quark
	  -I spm/include
	  -I include/lib"

cpp -x c -P -o "$PREPROCESSED_DTS" "$COMBINED_DTS" ${INCLUDES}
dtc -I dts -O dtb "$PREPROCESSED_DTS" > "$GENERATED_DTB"
