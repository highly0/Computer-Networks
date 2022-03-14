#! /bin/bash

export PYTHONUNBUFFERED=on

# How this script is called: 
# ./run_client.sh <program name> <server port> <test target> <num hashes> <data size>
# ./run_client.sh "CP Test" 41714 "/bin/cp" 10 1024

# Grab arguments from whoever called this script.
prog_name="$1" # Used just for tracking what passed/failed
shift
server_port="$1" # So we can communicate with the server
shift
test_target="$1" # The file we want to hash
shift
num_hashes="$1" # Number of HashRequests
shift
data_size="$1" # How large of chunks to send to server
shift

# References for directories
submission_dir="$(pwd)"

# Timeout period before killing client
timeout="10s"

# Temp files to store output
ref_out=$(mktemp -t  ${prog_name}XXX)
test_out=$(mktemp -t ${prog_name}XXX)

echo "INFO: Running program ${prog_name}"

# Define a function that cleans up after the test run.
function clean_up {
    rm "${ref_out}" "${test_out}"
}

# Ensure that the above function is called regardless of how the script exits.
# This most importantly includes natural causes and ctrl-c.
trap clean_up EXIT

# ----------- Actually start doing things -----------------

# This is ground-truth output.
docker run --rm -v "${submission_dir}:/opt" -w /opt baseline ./hash_sample.sh ${test_target} ${num_hashes} ${data_size} > ${ref_out}
hash_status=$?

if [[ 0 -ne ${hash_status} ]]; then
    echo "ERROR: Failed to compute ground truth with hash_sample.sh" >&2
    exit 1
fi

# Run the client, and kill it if it runs for too long
docker exec server /opt/client -a 127.0.0.1 -p ${server_port} -n ${num_hashes} --smin=${data_size} --smax=${data_size} -f ${test_target} > ${test_out}

# Now compare the ground-truth output with our test output.
diff ${ref_out} ${test_out}
status=$?

if [[ 0 -ne ${status} ]]; then
    echo "ERROR: Failed test case for ${prog_name}" >&2
    exit 1
else
    echo "INFO: Passed test case for ${prog_name}"
fi

# You passed, Congratulations!
echo "INFO: Test passed for ${prog_name}" >&2