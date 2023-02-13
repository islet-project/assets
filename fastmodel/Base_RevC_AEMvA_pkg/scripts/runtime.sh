# Source this script once before running the model

# Local function to prevent this sourced script
# from changing the environment variables
function get_directory_path()
{
    local SOURCE_FILE=`realpath ${BASH_SOURCE}`
    local DIR_PATH=`dirname ${SOURCE_FILE}`
    echo "${DIR_PATH}"
}

#Export SystemC home if it exists
if [ -d "$(get_directory_path)/../SystemC" ]; then
    export SYSTEMC_HOME=$(get_directory_path)/../SystemC
fi

#Check if fmtplib is in LD_LIBRARY_PATH
echo ${LD_LIBRARY_PATH} | grep fmtplib > /dev/null

#If it is not in LD_LIBRARY_PATH, then add it
if [ $? -ne 0 ]; then
    # If User has an LD_LIBRARY_PATH already set for other purposes
    if [ -n "${LD_LIBRARY_PATH}" ]; then
        # Then, include fmtplib at the begining of LD_LIBRARY_PATH
        # to make sure it will be the first path to be picked-up
        LD_LIBRARY_PATH="$(get_directory_path)/../fmtplib:${LD_LIBRARY_PATH}"
    else
        # else just set LD_LIBRARY_PATH with fmtplib path
        LD_LIBRARY_PATH="$(get_directory_path)/../fmtplib"
    fi
    # Finally, export it to be used by the Model
    export LD_LIBRARY_PATH=${LD_LIBRARY_PATH}
fi
