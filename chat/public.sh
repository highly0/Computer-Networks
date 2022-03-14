#! /bin/bash

submission_dir="$(pwd)"

REFERENCE_SERVERS=( #leave this line
    # follow instructions on elms/piazza to insert reference servers here.
); #leave this line

docker run --rm -v "${submission_dir}/assignment2:/opt" -w /opt baseline make
if [ 0 -ne $? ]
then
    echo "Make failed" >&2
    exit 1
fi

if [ -x assignment2/rserver ]
then
    echo "Testing rserver (< appear from the reference server, > appear from test server)" >&2
    server_port=$((20000 + ($RANDOM % 10000)))
	echo "${server_port}" > ./sport
    echo "Starting test server on port $server_port..."
    docker run --rm -d --name rserver -v "${submission_dir}:/opt" baseline /opt/assignment2/rserver -p ${server_port}
	
    sleep 1

    output=$(mktemp)

    echo "Running reference client..."
	docker exec rserver bash -c "/opt/input.sh | /opt/provided/client" > $output
	
	docker exec rserver /opt/output.sh
	
    diff ./refout $output
    rc=$?

    # Stop the container running the server
	echo "Stopping docker container..."
	docker stop rserver
	
    rm ./refout $output ./sport

    if [ -0 -ne $rc ]
    then
        echo "Incorrect output!" >&2
        exit 1
    else
        echo "Correct output"
    fi

elif [ -x assignment2/rclient ]
then
    echo "Testing rclient (< appear from the reference client, > appear from test client)" >&2

    if [ ${#REFERENCE_SERVERS[@]} -eq 0 ]
    then
        echo "No reference servers defined. Follow directions on elms to add them to your script." >&2
        exit 1
    fi

    server_num=$(($RANDOM % ${#REFERENCE_SERVERS[@]}))
	echo "${REFERENCE_SERVERS[$server_num]}" > ./saddr

    output=$(mktemp)
	reference=$(mktemp)

    echo "Running reference client..."
    docker run --rm -d --name rserver -v "${submission_dir}:/opt" baseline tail -f /etc/issue
    docker exec rserver bash -c "/opt/input.sh | /opt/provided/client" | sed -E 's/rand([0-9]+)/rand/' > $reference
    #docker run --rm -d --name rserver -v "${submission_dir}:/opt" baseline bash -c "/opt/input.sh | /opt/provided/client" | sed -E 's/rand([0-9]+)/rand/' > $reference
    #provided_pid=$!
    echo "Running test client..."
    docker exec rserver bash -c "/opt/input.sh | /opt/assignment2/client"| sed -E 's/rand([0-9]+)/rand/' > $output
    #rclient_pid=$!
    docker kill rserver
    cp $reference foo 
    cp $output bar

    diff $reference $output
    rc=$?

    rm $reference $output ./saddr

    if [ -0 -ne $rc ]
    then
        echo "Incorrect output!" >&2
        exit 1
    else
        echo "Correct output"
    fi

else
    echo "No rserver or rclient executable found" >&2
    exit 1
fi

git status | grep -E '\.[ch]$'
if [ 0 -eq $? ]
then
    echo "Uncommitted source code found" >&2
    echo "Add/commit these or remove them" >&2
    exit 1
fi

