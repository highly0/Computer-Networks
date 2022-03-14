#! /bin/bash

export PYTHONUNBUFFERED=on
submission_dir="$(pwd)"
assignment_dir="$(pwd)/assignment0"
client_executable="${assignment_dir}/client"
server_executable="${assignment_dir}/server"
server_pid=""

# Define a function that cleans up after the test run.
function clean_up {

    pkill -P $$

	# delete server container
	echo "INFO: Stopping server container"
	docker stop server
}

# Ensure that the above function is called regardless of how the script exits.
# This most importantly includes natural causes and ctrl-c.
trap clean_up EXIT


function start_server {
    # Generate a random port that's hopefully not taken already.
    server_port=$((20000 + ($RANDOM % 10000)))

    # Run the server (in the background).
    docker run --rm -d --name server -v "${assignment_dir}:/opt" baseline /opt/server -p ${server_port}

    # Capture the server's PID/PGID so that we may kill it later.
    # server_pid="-$!"
    # Wait for the server to start-up.
    # If your server takes more than a second, you should make it faster...
    sleep 1
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

# Ask git for a list of changes (including untracked files) and look for source files.
git status -u | grep -E '\.(c|h|py|java|erl)$'
if [[ 0 -eq $? ]]; then
    echo "WARNING: Uncommitted source code found, Add/commit these or remove them" >&2
fi

# Below is an array, elements are separated by spaces
# Since we can't have arrays of arrays, we'll just read in blocks of 4
# NOTE: The name of the program CANNOT have ANY spaces
# <name of program or test> <file to read> <number of hashes> <size of data chunks> 
prog_args=("CP_Test" "/bin/cp" 10 1024 "LS_Test" "/bin/ls" 10 1024 "MV_Test" "/bin/mv" 10 1024)

# Keep track of the children processes (the test cases) running
pids=()

# So we can match up PID->Program name
child_names=()

# Start up the server
start_server

# Passed all test?
passed_all=0

# Loop over the prog_args array and run each test case. Track the pids
pid_count=0
for (( i=0; i<${#prog_args[@]}; i+=4)); do
    echo "Running client: ${prog_args[$i]} ${server_port} ${prog_args[$i+1]} ${prog_args[$i+2]} ${prog_args[$i+3]}"

    ./run_client.sh ${prog_args[$i]} ${server_port} ${prog_args[$i+1]} ${prog_args[$i+2]} ${prog_args[$i+3]} &
    pids[${pid_count}]=$!

    child_names[${pid_count}]=${prog_args[$i]}
    ((pid_count++))
done

# Wait for every child process to finish
for (( k=0; k<${#pids[@]}; k++)); do
    wait ${pids[$k]}
    status=$?
    if [[ 0 -ne ${status} ]]; then
        echo "ERROR: Client program ${child_names[k]} failed" >&2
        passed_all=1
    fi
done

# One of the clients failed
if [[ 0 -ne ${passed_all} ]]; then
    echo "ERROR: Not all tests passed" >&2
    exit 1
fi

# You passed, Congratulations!
echo "INFO: All tests have passed!"
