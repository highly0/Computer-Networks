#! /bin/bash

export PYTHONUNBUFFERED=on
assignment_dir="./assignment1"
server_executable="${assignment_dir}/server"
client_executable="${assignment_dir}/client"
timeout="10s"
ctr="udp_test"

# Define a function that cleans up after the test run.
function clean_up {
    # Ensure child processes are dead
    pkill -P $$ 

    # Shut down the container
    echo "INFO: Shutting down the container... This may take a few seconds"
    docker stop ${ctr}
}

# Ensure that the above function is called regardless of how the script exits.
# This most importantly includes natural causes and ctrl-c.
trap clean_up EXIT

function setup_network {
    # Grab three arguments from whoever called this function.
    delay="${1}ms"

    docker run -d --rm --privileged --name ${ctr} -v "$(pwd):/opt" -u root baseline tail -f /etc/issue
    sleep 2

    # Set up this test scenario given the specified link delays
    docker exec ${ctr} /opt/start_testbed.sh ${delay} 1Gbit 0.0001

    # Ping between containers to warm the ARP cache
    docker exec ${ctr} ip netns exec foo ping -c 2 1.2.3.5
}

function start_server {
    drop_rate=${1}

    echo "INFO: Starting Server"

    # Run the server (in the background).
    docker exec -d ${ctr} ip netns exec foo /opt/${server_executable} -p 1234 -d ${drop_rate}

    # If you want to gather the output of your server, use below line line instead and comment line 45 
    # docker exec -t ${ctr} ip netns exec foo /opt/${server_executable} -p 1234 -d ${drop_rate} > temp.out &

    # Wait for the server to start-up.
    # If your server takes more than a second, you should make it faster...
    sleep 1
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

# Ask git for a list of changes (including untracked files) and look for source files.
git status -u | grep -E '\.(c|h|py|java|erl)$'
if [[ 0 -eq $? ]]; then
    echo "WARNING: Uncommitted source code found, Add/commit these or remove them" >&2
fi

# Setup the virtual network namespaces and interfaces for the tests
# setup_network <delay of network>
setup_network 30

# Start the server up
# start_server <drop rate %>
drop_rate=50
start_server ${drop_rate}

# List of test cases, defined as count, drop_rate, client->server ms, server->client ms
# List of clients to run, arguments are in groups of 3 as follows:
# <output filename> <number of requests> <timeout for client>
prog_args=("client1.out" 25 10 "client2.out" 50 5)

pids=()
pid_count=0

# Run all the clients
echo "INFO: Starting clients..."
for (( i=0; i<${#prog_args[@]}; i+=3 )); do
    # You can remove the `&` at the end if you want to run sequentially instead - though I didn't full test this. May want to remove the pid stuff
    ./run_client.sh ${prog_args[$i]} ${prog_args[$i+1]} ${prog_args[$i+2]} ${drop_rate} ${delay} &
    pids[${pid_count}]=$!

    ((pid_count++))
done

# Wait for the client scripts to finish before exiting
echo "INFO: Waiting on clients to finish..."
for (( k=0; k<${#pids[@]}; k++)); do
    wait ${pids[$k]} 
done