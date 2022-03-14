#! /bin/bash

# This must be run as root. If using docker, the container must be privileged.

# Link characteristics. Feel free to change these!
delay=${1:-'10ms'}
rate=${2:-'1kbit'}
loss=${3:-'50'}

ns1='foo'
ns2='bar'

ip link add ${ns1} type veth peer name ${ns2}
ip netns add ${ns1}
ip netns add ${ns2}
ip link set ${ns1} netns ${ns1}
ip link set ${ns2} netns ${ns2}
ip netns exec ${ns1} ip link set lo up
ip netns exec ${ns2} ip link set lo up
ip netns exec ${ns1} ip addr add 1.2.3.4/32 dev ${ns1}
ip netns exec ${ns2} ip addr add 1.2.3.5/32 dev ${ns2}
ip netns exec ${ns1} ip link set ${ns1} up
ip netns exec ${ns2} ip link set ${ns2} up
ip netns exec ${ns1} ip route add 1.2.3.0/24 dev ${ns1} proto static scope global src 1.2.3.4
ip netns exec ${ns2} ip route add 1.2.3.0/24 dev ${ns2} proto static scope global src 1.2.3.5
ip netns exec ${ns1} tc qdisc add dev ${ns1} root handle 1:0 netem
ip netns exec ${ns2} tc qdisc add dev ${ns2} root handle 1:0 netem
ip netns exec ${ns1} tc qdisc change dev ${ns1} root netem delay ${delay} rate ${rate} loss random ${loss}
ip netns exec ${ns1} tc qdisc change dev ${ns1} root netem delay ${delay} rate ${rate} loss random ${loss}
