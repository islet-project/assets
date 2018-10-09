#! /bin/bash

#
# Script to run the EL3 payload on the Foundation FVP.
#
# /!\ The EL3 payload is not supported on the Foundation FVP without tweaking
# the code. You need to modify the number of expected cores in
# plat/fvp/platform.h:
#  -#define CPUS_COUNT             8
#  +#define CPUS_COUNT             4
#

set -e

# usage: check_file_is_present <filename>
# Check that <filename> exists in the current directory.
# If not, print an error message and exit.
function check_file_is_present
{
	BIN_FILE=$1
	if [ ! -e "$BIN_FILE" ]; then
		echo "ERROR: Can't find \"$BIN_FILE\" file"
		echo "Please copy $BIN_FILE into the current working directory."
		echo "Alternatively, a symbolic link might be created."
		echo
		exit 1
	fi
}

check_file_is_present "bl1.bin"
check_file_is_present "fip.bin"

# Create an 8-byte file containing all zero bytes.
# It will be loaded at the beginning of the Trusted SRAM to zero the mailbox.
MAILBOX_FILE=mailbox.dat
rm -f $MAILBOX_FILE
dd if=/dev/zero of=$MAILBOX_FILE bs=1 count=8

# The path to the Foundation model must be provided by the user.
MODEL_EXEC="${MODEL_EXEC:?}"
MODEL_PARAMETERS="				\
	--cores=4				\
	--visualization				\
	--data=bl1.bin@0x0			\
	--data=fip.bin@0x08000000		\
	--data=$MAILBOX_FILE@0x04000000		\
        --data=build/fvp/el3_payload.bin@0x80000000 \
"

echo $MODEL_EXEC $MODEL_PARAMETERS
$MODEL_EXEC $MODEL_PARAMETERS
