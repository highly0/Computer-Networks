#! /bin/bash
if [ -x /opt/assignment2/rserver ]
then
	input=$(mktemp)
	server_port=$(cat /opt/sport)
	
	# Change these lines to build your own test script if you implemented the server
	echo "\\\\connect 127.0.0.1:$server_port" >> $input
	echo "\\\\quit" >> $input


	while read line
	do
	# Send the next line of input to the client.
	  echo ${line}
	  # Wait a couple of seconds, to let things process through.
	  sleep 2
	done <${input}

	rm ${input}
elif [ -x /opt/assignment2/rclient ]
then
	input=$(mktemp)
	reference_server=$(cat /opt/saddr)
	
	# Change these lines to build your own test script if you implemented the client
    echo "\\\\connect ${reference_server}" >> $input
    echo "\\\\quit" >> $input
	
	while read line
	do
	# Send the next line of input to the client.
	  echo ${line}
	  # Wait a couple of seconds, to let things process through.
	  sleep 2
	done <${input}
	
	rm ${input}
fi
