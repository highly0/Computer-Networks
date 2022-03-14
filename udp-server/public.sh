#! /bin/bash

export PYTHONUNBUFFERED=on
assignment_dir="./assignment1"
server_executable="${assignment_dir}/server"
client_executable="${assignment_dir}/client"
timeout="10s"
server_pid=""
ctr="udp_test"

# Define a function that cleans up after the test run.
function clean_up {
    docker kill ${ctr}
}

# Ensure that the above function is called regardless of how the script exits.
# This most importantly includes natural causes and ctrl-c.
trap clean_up EXIT

# Function called on each test case that we test against.
function test_case {
    # Grab three arguments from whoever called this function.
    requests=$1
    shift
    drop_rate=$1
    shift
    delay="${1}ms"
    shift

    docker run -d --rm --privileged --name ${ctr} -v "$(pwd):/opt" -u root baseline tail -f /etc/issue
    sleep 2

    # Set up this test scenario given the specified link delays
    docker exec ${ctr} /opt/start_testbed.sh ${delay} 1Gbit 0.0001

    # Ping between containers to warm the ARP cache
    docker exec ${ctr} ip netns exec foo ping -c 2 1.2.3.5

    echo "INFO: Running test"

    # Run the server (in the background).
    docker exec -d ${ctr} ip netns exec foo /opt/${server_executable} -p 1234 -d ${drop_rate}
    # Wait for the server to start-up.
    # If your server takes more than a second, you should make it faster...
    sleep 1

    # Run the client, and kill it if it runs for too long
    docker exec ${ctr} ip netns exec bar timeout -s 9 ${timeout} /opt/${client_executable} -a 1.2.3.4 -p 1234 -n ${requests} -t 5

    # Stop the container.
    docker kill ${ctr}

    # Compute expected values based on test case parameters
    expected_drop=$(echo ${drop_rate} ${requests} | awk '{print ($1*$2)/100;}')
    expected_theta=0
    #expected_theta=$(echo "scale=4; (${client_server_delay}-${server_client_delay})/2000" | bc)
    expected_delta=$(echo ${delay} | awk '{print $1/500;}') #"scale=4; (${delay})/500" | bc)

    # Defer to you to validate the results
    echo "Are the values in the theta column close to ${expected_theta}?"
    echo "Are the values in the delta column close to ${expected_delta}?"
    echo "Did you detect roughly ${expected_drop} dropped packets?"
    echo "If so, great! If not, keep working on it."
}

# If there's a makefile present, clean and build the assignment.
if [[ -f "${assignment_dir}/makefile" || -f "${assignment_dir}/Makefile" ]]; then
    docker run --rm -v "$(pwd):/opt" -u root baseline make -C /opt/assignment1 clean
    docker run --rm -v "$(pwd):/opt" -u root baseline make -C /opt/assignment1
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

#echo "INFO: Starting testbed"
#start_testbed.py -f testbed.yml

# List of test cases, defined as count, drop_rate, client->server ms, server->client ms
test_case 20 50 30

# Ask git for a list of changes (including untracked files) and look for source files.
git status -u | grep -E '\.(c|h|py|java|erl)$'
if [[ 0 -eq $? ]]; then
    echo "WARNING: Uncommitted source code found, Add/commit these or remove them" >&2
fi
