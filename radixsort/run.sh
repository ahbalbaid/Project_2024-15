#!/bin/bash

arr1=(65536 262144 1048576 4194304 16777216 67108864 268435456)
arr2=(128 256 512)

for val1 in "${arr1[@]}"; do
  for val2 in "${arr2[@]}"; do
    sbatch mpi.grace_job "$val1" "$val2" 1
  done
done