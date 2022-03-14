! /bin/bash

# Grab the arguments
outfilename=${1}
requests=${2}
cl_timeout=${3}
drop_rate=${4}
delay=${5}

# Predefined Variables
ctr="udp_test"
assignment_dir="./assignment1"
client_executable="${assignment_dir}/client"
timeout="10s"

echo "INFO: Running test, output to ${outfilename}"

# Run the client
# NOTE: I removed the timeout for the process because it would prematurely kill it due to dropped packets
docker exec -t ${ctr} ip netns exec bar /opt/${client_executable} -t ${cl_timeout} -a 1.2.3.4 -p 1234 -n ${requests} > ${outfilename} 2>&1

# Compute expected values based on test case parameters
expected_drop=$(echo ${drop_rate} ${requests} | awk '{print ($1*$2)/100;}') >> ${outfilename}
expected_theta=0
expected_delta=$(echo ${delay} | awk '{print $1/500;}') >> ${outfilename}

# How many packets printed out
total_count=$(cat ${outfilename} | wc -l)

# Count how many lines say 'Dropped'
drop_count=$(grep -ow 'Dropped' ${outfilename} | wc -l)

# Percentage dropped calculated
drop_pct=$(echo "${drop_count} ${total_count}" | awk '{print ($1/$2)*100;}')

# Defer to you to validate the results
echo "Are the values in the theta column close to ${expected_theta}?" >> ${outfilename}
echo "Are the values in the delta column close to ${expected_delta}?" >> ${outfilename}
echo "Out of ${total_count} packets sent, the server dropped ${drop_count} packets or roughly ${drop_pct}%" >> ${outfilename}
echo "Did you detect roughly ${expected_drop} dropped packets?" >> ${outfilename}
echo "If so, great! If not, keep working on it." >> ${outfilename}

echo "Finished writing to ${outfilename}, it contains output for the client."