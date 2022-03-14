# Parse the command line arguments
hosted=0
wireshark=0

# -h or --hosted will use the hosted server
# -i <file> or --inputfile <file> will use an input.sh-like script to input text into the client
# -w or --wireshark will use tcpdump and open the dump with Wireshark automatically
# -p <port number> or --port <port number> will run your server with it, does not apply to hosted server

while [[ $# -gt 0 ]]; do
    key="$1"

    case $key in
    -h|--hosted)
    hosted=1
    shift
    ;;
    -i|--inputfile)
    inputfile="$2"
    shift
    shift
    ;;
    -w|--wireshark)
    wireshark=1
    shift
    ;;
    -p|--port)
    server_port="$2"
    shift
    shift
    ;;
    *)
    echo "Unknown command line argument"
    exit 1
    shift
    ;;
esac
done


if [[ 0 == ${hosted} ]] 
then
    echo "Starting up the server..."

    # Startup the server
    docker run --rm -d --net=host --name ${con_name} -v "$(pwd)":/opt baseline sudo /opt/assignment2/rserver -p ${server_port}

    # If you want to capture the output of your server, use the line below instead of the one above
    # docker run --rm -t --net=host --name ${con_name} -v "$(pwd)":/opt baseline sudo /opt/assignment2/rserver -p ${server_port} &> server.out &

    # Wait for the server to start
    sleep 2

    # Start the tcpdump listening on localhost on the docker
    docker exec -d -t ${con_name} sudo tcpdump -U -i lo -w "/opt/${tcpdump_file}"

    if [[ -x "${inputfile}" ]]
    then
        echo "Feeding output data from ${inputfile} into client..."
        # Alike the public.sh, just feed the input into client
        docker exec ${con_name} bash -c "/opt/${inputfile} | /opt/provided/client"
    else
        echo "Entering the container as a client... Start entering your commands"
        # Start the client in interactive mode
        docker exec -ti ${con_name} /opt/provided/client
    fi

    wireshark_filter="tcp.port == ${server_port}"

    # Close down the container
    docker stop ${con_name}

else
    wireshark_filter="ip.addr == 128.8.130.3"
    
    if [[ 1 == ${wireshark} ]]
    then
        tcpdump -U -i any -w "${tcpdump_file}" &
    fi

    if [[ -x "${inputfile}" ]]
    then
        # NOTE: This requires changing the ip address in ./input.sh or whatever script you're using
        echo "Feeding output data from ${inputfile} into client..."
        docker run --rm -ti --name ${con_name} --net=host -v "$(pwd)":/opt baseline bash -c "/opt/${inputfile} | /opt/provided/client"
    else
        echo "Entering the container as a client... Start entering your commands"
        docker run --rm -ti --name ${con_name} --net=host -v "$(pwd)":/opt baseline /opt/provided/client
    fi

    # Want to stop the tcpdump process
    pkill -f tcpdump
fi


if [[ 1 == ${wireshark} ]]
then
    # Copy the dump file to somewhere else
    # Comment if this is NOT applicable to you
    cp ${tcpdump_file} ${cp_dir}

    # !!!!!! CHANGE IF NECESSARY !!!!!!

    # WSL/Windows:
    #"${wireshark_dir}" -r "${final_pcap_location}" -R "${wireshark_filter}"

    # Mac: (UNTESTED)
     open ${cp_dir}/${tcpdump_file} -a "${wireshark_dir}" -r "${final_pcap_location}" -R "${wireshark_filter}"

    # Linux: (UNTESTED)
    # xdg-open ${cp_dir}/${tcpdump_file}
fi