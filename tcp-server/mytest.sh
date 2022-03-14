#! /bin/bash

export PYTHONUNBUFFERED=on
submission_dir="$(pwd)"
assignment_dir="$(pwd)/assignment0"
client_executable="${assignment_dir}/client"
server_executable="${assignment_dir}/server"
port=$((20000 + ($RANDOM % 10000)))
num_hashes=1000
data_size=500
src=/bin/bash

function compile {
    # If there's a makefile present, clean and build the assignment.
    if [[ -f "${assignment_dir}/makefile" || -f "${assignment_dir}/Makefile" ]]; then
        # docker run --rm -v "${assignment_dir}:/opt" -w /opt baseline make clean
        # docker run --rm -v "${assignment_dir}:/opt" -w /opt baseline make
        cd ${assignment_dir}; make clean; make
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
}

function clean_up {
	pkill server
    cd ${assignment_dir}/; make clean
}

trap clean_up EXIT

compile

 # run server
./server -p ${port} &

# run 10 clients
for i in $(seq 10)
do
    echo "exec client ${i}"; ./client -a 127.0.0.1 -p ${port} -n ${num_hashes} --smin=${data_size} --smax=${data_size} -f ${src} > /dev/null
done