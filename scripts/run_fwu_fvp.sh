#! /bin/bash

set -e

function check_file {
    if [ -e "$1" ]; then
	return 0
    else
	echo "File $1 does not exist."
	return 1
    fi
}

# Check the presence of all required files.
check_file bl1.bin
check_file ns_bl1u.bin
check_file fip.bin
check_file fwu_fip.bin
check_file backup_fip.bin ||
    (echo "Creating backup_fip.bin as a copy of fip.bin." ;
     cp fip.bin backup_fip.bin)

# Chosen topology: 2 clusters of 4 cores each.
# Power on core 0 only.
# Load all binaries at the right addresses.
FVP_Base_AEMv8A-AEMv8A					\
    -C cluster0.NUM_CORES=4				\
    -C cluster1.NUM_CORES=4				\
    -C pctl.startup=0.0.0.0				\
    -C bp.secureflashloader.fname=bl1.bin		\
    -C bp.flashloader0.fname=fip.bin			\
    --data cluster0.cpu0=backup_fip.bin@0x09000000	\
    --data cluster0.cpu0=fwu_fip.bin@0x08400000		\
    --data cluster0.cpu0=ns_bl1u.bin@0x0beb8000
