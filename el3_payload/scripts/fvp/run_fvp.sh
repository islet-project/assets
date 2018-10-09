#! /bin/bash

set -e

UART_OUTPUT_FILE=uart0.log

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

# The path to the Foundation model must be provided by the user.
MODEL_EXEC="${MODEL_EXEC:?}"
MODEL_PARAMETERS="
	-C pctl.startup=0.0.*.*,0.1.*.*					\
	-C bp.secureflashloader.fname=bl1.bin				\
	-C bp.flashloader0.fname=fip.bin				\
	--data cluster0.cpu0=build/fvp/el3_payload.bin@0x80000000	\
	-C bp.secureSRAM.fill1=0x00000000				\
	-C bp.secureSRAM.fill2=0x00000000				\
	-C bp.pl011_uart0.out_file=$UART_OUTPUT_FILE			\
	-C bp.pl011_uart0.shutdown_on_eot=1				\
"

echo $MODEL_EXEC $MODEL_PARAMETERS
$MODEL_EXEC $MODEL_PARAMETERS

# Print results
green='\033[1;32;40m'
no_color='\033[0m'
echo
echo -e "$green"
echo "============"
echo " COMPLETE!"
echo "============"
echo -e "$no_color"
echo "UART output:"
echo "--------------------------------8<------------------------------"
cat $UART_OUTPUT_FILE
echo "--------------------------------8<------------------------------"
echo
echo "Output saved in $UART_OUTPUT_FILE file."
echo
