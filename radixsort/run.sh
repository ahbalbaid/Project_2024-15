#!/bin/bash

arr1=(65536 524288 4194304)
arr2=(2 4 8 16 32 64)

for val1 in "${arr1[@]}"; do
  for val2 in "${arr2[@]}"; do
    sbatch mpi.grace_job "$val1" "$val2"
  done
done