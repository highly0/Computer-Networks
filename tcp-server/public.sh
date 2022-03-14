#! /bin/bash

export PYTHONUNBUFFERED=on
submission_dir="$(pwd)"
assignment_dir="$(pwd)/assignment0"
client_executable="${assignment_dir}/client"
server_executable="${assignment_dir}/server"
timeout="10s"
ref_out=$(mktemp)
test_out=$(mktemp)
server_pid=""

# Define a function that cleans up after the test run.
function clean_up {
    rm "${ref_out}" "${test_out}"
	# delete server container
	echo "INFO: Stopping server container"
	docker stop server
	#docker rm server
}

# Ensure that the above function is called regardless of how the script exits.
# This most importantly includes natural causes and ctrl-c.
trap clean_up EXIT

# Function called on each test case that we test against.
function test_case {
    # Grab three arguments from whoever called this function.
    test_target="$1"
    shift
    num_hashes="$1"
    shift
    data_size="$1"
    shift

    # Generate a random port that's hopefully not taken already.
    server_port=$((20000 + ($RANDOM % 10000)))

    # This is ground-truth output.
    docker run --rm -v "${submission_dir}:/opt" -w /opt baseline ./hash_sample.sh ${test_target} ${num_hashes} ${data_size} > ${ref_out}

    # Tell job control to put child processes in a different process group.
    # This allows us to kill all children of the server process at once.
    # set -m

    # Run the server (in the background).
	docker run --rm -d --name server -v "${assignment_dir}:/opt" baseline /opt/server -p ${server_port}
    # Capture the server's PID/PGID so that we may kill it later.
    # server_pid="-$!"
    # Wait for the server to start-up.
    # If your server takes more than a second, you should make it faster...
    sleep 1

    # Tell job control to use our process group (undoes set -m)
    # set +m

    # Run the client, and kill it if it runs for too long
    docker exec server /opt/client -a 127.0.0.1 -p ${server_port} -n ${num_hashes} --smin=${data_size} --smax=${data_size} -f ${test_target} > ${test_out}

    # Stop the server with the PGID we captured earlier.
    #kill -s 9 "${server_pid}"
    #server_pid=""

    # Now compare the ground-truth output with our test output.
    diff ${ref_out} ${test_out}
    status=$?

    if [[ 0 -ne ${status} ]]; then
        echo "ERROR: Failed test case" >&2
        exit 1
    else
        echo "INFO: Passed test case" >&2
    fi
}

# If there's a makefile present, clean and build the assignment.
if [[ -f "${assignment_dir}/makefile" || -f "${assignment_dir}/Makefile" ]]; then
    docker run --rm -v "${assignment_dir}:/opt" -w /opt baseline make clean
    docker run --rm -v "${assignment_dir}:/opt" -w /opt baseline make
    if [ 0 -ne $? ]; then
        echo "ERROR: Make failed" >&2
        exit 1
    else
        echo "INFO: Built correctly with make" >&2
    fi
else
    echo "ERROR: No makefile found" >&2
    exit 1
fi

# Ensure the server and client executables exist and are, in fact, executable.
if [ ! -x "${server_executable}" ]
then
    echo "ERROR: No executable found at '${server_executable}'"
    exit 1
fi

if [ ! -x "${client_executable}" ]
then
    echo "ERROR: No executable found at '${client_executable}'"
    exit 1
fi

# List of test cases, defined as filename, num_hashes, hash_size
test_case "/bin/cp" 10 1024

# Ask git for a list of changes (including untracked files) and look for source files.
git status -u | grep -E '\.(c|h|py|java|erl)$'
if [[ 0 -eq $? ]]; then
    echo "WARNING: Uncommitted source code found, Add/commit these or remove them" >&2
fi

# You passed, Congratulations!
echo "INFO: Test passed" >&2
