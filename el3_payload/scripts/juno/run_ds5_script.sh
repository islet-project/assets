#! /bin/sh

set -e

# Expect the script to run in argument
if [ $# != 1 ]; then
	echo "ERROR: No script provided"
	echo "usage: $(basename $0) <ds5_script_to_run>"
	exit 1
fi

# Is DS-5 command-line debugger found?
if [ ! $(which debugger) ]; then
	echo 'ERROR: Failed to find DS-5 command-line debugger.'
	echo 'Please add the path to the command-line debugger in your PATH.'
	echo 'E.g.: export PATH=<DS-5 install dir>/bin:$PATH'
	exit 1
fi

# DS-5 configuration database entry for Juno r0
juno_cdb_entry='ARM Development Boards::Juno ARM Development Platform (r0)::Bare Metal Debug::Bare Metal Debug::Debug Cortex-A53_0::DSTREAM'

# Browse for available DSTREAM connections and lists targets that match the
# connection type specified in the configuration database entry
echo "Trying to detect your DSTREAM unit..."
connections_list=available_connections
debugger --cdb-entry "$juno_cdb_entry" --browse \
  | tee $connections_list

# Remove first line in the file (i.e. "Available connections:")
tail -n +2 $connections_list > ${connections_list}_stripped
mv ${connections_list}_stripped ${connections_list}

# Use first available connection
read connection < $connections_list || true
rm $connections_list

if [ -z "$connection" ] ; then
  echo "ERROR: Found no connection"
  exit 1
fi

# Run DS-5 script
echo "Connecting to $connection..."
debugger \
	--cdb-entry "$juno_cdb_entry" \
	--cdb-entry-param "Connection=$connection" \
	--stop_on_connect=false \
	--script=$1
