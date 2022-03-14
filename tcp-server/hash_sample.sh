#! /bin/bash

target_file=$1; shift
num_hashes=$1; shift
data_size=$1; shift

max_hash=$(( ${num_hashes} - 1 ))

for i in $(seq 0 ${max_hash})
do
    hash_val=$(dd if=${target_file} bs=${data_size} skip=$i count=1 2>/dev/null | shasum -a 256 | awk '{print $1}')
    echo "$i: 0x${hash_val}"
done

