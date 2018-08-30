#! /bin/bash

set -e

# Expect the script to run in argument
if [ $# != 1 ]; then
	echo "ERROR: No script provided"
	echo "usage: $(basename $0) <armds_script_to_run>"
	exit 1
fi

# Is DS-5 command-line debugger found?
if [ ! $(which armdbg) ]; then
	echo 'ERROR: Failed to find DS-5 command-line debugger.'
	echo 'Please add the path to the armdbg program in your PATH.'
	echo 'E.g.: export PATH=<DS-5 install dir>/bin:$PATH'
	exit 1
fi

# DS-5 configuration database entry for Juno r0
juno_cdb_entry='Arm::Juno Arm Development Platform (r0)::Bare Metal Debug::Bare Metal Debug::Debug Cortex-A53_0::DSTREAM'

# Browse for available DSTREAM connections and lists targets that match the
# connection type specified in the configuration database entry
echo "Trying to detect your DSTREAM unit..."
connections_list=available_connections
armdbg --cdb-entry "$juno_cdb_entry" --browse \
  | tee $connections_list

# Remove first line in the file (i.e. "Available connections:")
tail -n +2 $connections_list > ${connections_list}_stripped
mv ${connections_list}_stripped ${connections_list}

if [ ! -s $connections_list ] ; then
	echo "ERROR: Found no connection"
	exit 1
fi

# Ask the user which connection to use.
echo
cat -n $connections_list
echo -n "Which one do you want to connect to? "
read connection_id

# Extract the corresponding connection name from the file.
connection=$(((sed -n "${connection_id}p") | sed 's/^ *//') < $connections_list)
if [ -z "$connection" ] ; then
	echo "ERROR: Invalid connection"
	exit 1
fi

rm $connections_list

# Run DS-5 script
echo
echo "Connecting to $connection..."
armdbg \
	--cdb-entry "$juno_cdb_entry" \
	--cdb-entry-param "Connection=$connection" \
	--stop_on_connect=false \
	--script=$1
