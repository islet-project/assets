#!/bin/bash

#
# Copyright (c) 2018, Arm Limited. All rights reserved.
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

echo "\
/ {
	memory_regions {
		text {
			str = \"Text\";
			base = <0x0 ${TEXT_START}ULL>;
			size = <0x0 (${TEXT_END}ULL - ${TEXT_START}ULL)>;
			attr = <RD_MEM_NORMAL_CODE>;
		};
		rodata {
			str = \"RO Data\";
			base = <0x0 (${RODATA_START}ULL)>;
			size = <0x0 (${RODATA_END}ULL - ${RODATA_START}ULL)>;
			attr = <RD_MEM_NORMAL_RODATA>;
		};
		rwdata {
			str = \"Data\";
			base = <0x0 ${DATA_START}ULL>;
			size = <0x0 (${DATA_END}ULL - ${DATA_START}ULL)>;
			attr = <RD_MEM_NORMAL_DATA>;
		};
		bss {
			str = \"BSS\";
			base = <0x0 ${BSS_START}ULL>;
			size = <0x0 (${BSS_END}ULL - ${BSS_START}ULL)>;
			attr = <RD_MEM_NORMAL_BSS>;
		};
	};
};" > "$EXTRA_DTS"

cat "$ORIGINAL_DTS" "$EXTRA_DTS" > "$COMBINED_DTS"

INCLUDES="-I spm/cactus
	  -I spm/ivy
	  -I spm/include
	  -I include/lib"

cpp -x c -P -o "$PREPROCESSED_DTS" "$COMBINED_DTS" ${INCLUDES} -DAARCH64
dtc -I dts -O dtb "$PREPROCESSED_DTS" > "$GENERATED_DTB"
