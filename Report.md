# CSCE 435 Group project

## 0. Group number: 15

## 1. Group members:
1. Edgar Garcia-Doria
2. Abdullah Balbaid
3. Sahil Dhana
4. Andy Callahan

## 2. Project topic: Parallel sorting algorithms

### 2a. Description
Our team will communicate through Discord. We'll create dedicated channels for different aspects of the project to keep our discussions organized. Regular voice or video meetings will be scheduled to sync up on progress, address any issues, and plan next steps. By using Discord, we'll be able to share files, code snippets, and updates instantly, ensuring everyone stays on the same page throughout the project.

We will emplement:
- Bitonic Sort: A bitonic sequence is a series of elements that are monotonically increasing then decreasing. The algorithm forms a bitonic sequence from an unordered sequence starting with a size of two (default bitonic). Then, the algorithm recursively merges pairs of bitonic sequences together to sort the elements.
- Sample Sort: Sorting algorithm that is a generalization of quicksort. It divides an array into small segments then sorts each segment. Once each segment is sorted it uses “samples” from each segment to determine partition boundaries/splitters, which then distributes all the data into sorted buckets. Buckets are then sorted independently and merged to create a final sorted array, which makes it an appropriate algorithm for parallel processing.
- Merge Sort: Sorting algorithm that uses the divide-and-conquer strategy. It splits the array into smaller halves, continues to divide the halves until each subarray has only one item, and finally compares the smaller subarrays and combines their results into a sorted array.
- Radix Sort: Sorting algorithm that processes integers by individual digits. It groups numbers based on each digit, starting from the least significant digit to the most significant, and continues sorting and redistributing them accordingly. In a parallel implementation, it divides the dataset among multiple processors; each processor sorts its portion for each digit position and exchanges data based on counts, repeating this process until the entire array is sorted.

### 2b. Pseudocode for each parallel algorithm
Merge sort  
```
Function mergeSort(array a, temp array b, left, right):
    If left < right:
        mid = (left + right) / 2
        Call mergeSort(a, b, left, mid)
        Call mergeSort(a, b, mid + 1, right)
        Call merge(a, b, left, mid, right)

Function merge(array a, temp array b, left, mid, right):
    Initialize h = left, i = left, j = mid + 1
    While h <= mid and j <= right:
        If a[h] <= a[j], set b[i] = a[h] and increment h
        Else set b[i] = a[j] and increment j
        Increment i
    Copy remaining elements from a[j to right] or a[h to mid] into b[i to end]
    Copy elements from b[left to right] back to a

Main:
    Allocate original_array of size n

    Initialize MPI
    Get world_rank and world_size

    Calculate size = n / world_size (subarray size for each process)
    Allocate sub_array for each process

    Scatter original_array to all processes:
        MPI_Scatter(original_array, size, sub_array, size, 0)

    Perform mergeSort on sub_array:
        Allocate tmp_array
        Call mergeSort(sub_array, tmp_array, 0, size - 1)

    Gather sorted subarrays at rank 0:
        MPI_Gather(sub_array, size, sorted_array, size, 0)

    If world_rank == 0:
        Allocate other_array
        Call mergeSort on sorted_array to combine all parts into a final sorted array
        Print sorted array

    Free allocated memory for original_array, sub_array, tmp_array
    Finalize MPI
```

Bitonic sort  
```
Main:
  Initialize MPI 
  Get number of processes and current rank
  Create initial input array
  Scatter array in chunks to worker processes
  Call bitonic_sort on each process
  When sorting has finished gather all sub-arrays

Function bitonic_sort():
  Sequential sort the local data (quicksort)
  Let d = number of stages (log_2 (number of processors))
  for i:=0 to d - 1 do:
    for j:=i down to 0 do
      if (i+1) bit of process rank = jth bit of rank then
        comp_exchange_max(j)
      else
        comp_exchange_min(j)

Function comp_exchange_max(j)
	Paired process rank = current rank xor 1 << j
	Send elements to paired process
	Receive elements from paired process
	Merge the higher elements

Function comp_exchange_min(j)
	Paired process rank = current rank xor 1 << j
	Send elements to paired process
	Receive elements from paired process
	Merge the lower elements
```

Radix Sort
```
Main:
  Initialize MPI
  Get number of processes and current rank
  Create initial input array
  Scatter array in chunks to worker processes
  Call radix_sort on each process
  When sorting has finished gather all sub-arrays

Function radix_sort(local_data, num_procs, rank):
  max_num = max(local_data)
  num_digits = len(str(max_num))

  for digit in range(num_digits):
    
    Initialize array (count) of size 10 to zero

    for number in local_data:
      # Extract digit from (digit) position of the number and increment count array
      count[int(str(number)[digit])]++

    global_count = reduce(global_count, count)
    Get and Compute global offsets (global_offsets)

    for number in local_data:
      # Extract digit from (digit) position of the number
      number_digit = int(str(number)[digit])
      Get target processor for this number based on (global_offsets)
      Send the number to its target processor
    
    Receive numbers from other processes and update (local_data)
```
Sample Sort
```
function Sample_Sort(arr[1..n], num_buckets, p):
        1. Initialize MPI
        - MPI.Init()
        - Get rank of the current process and num_processes
        - Get the segment size -> segment_size = n / num_processes


        2. Scatter the array A across all processes
        - local_segment = MPI_Scatter(A, segment_size, root=0)
        - Processes now have a portion of arr('local_segment')


        3. Sort the local segment using a appropriate sorting algorithm 
        - Sort local_segment


        4. Select S samples from local segments
        - samples_local = []
        - Randomly select '(p−1) * num_buckets' elements from local_segment as samples
        - samples_all = MPI_Gather(samples_local, root=0) - gathers all samples at root processes


        5. Root process sorts samples and selects splitters
        - if rank == 0:
            - Sort 'samples_all' using a appropriate sorting algorithm 
            - Determine splitters: [s0, s1, ..., sp] <- [-∞, Sk, S2k, ..., S(p−1)k, ∞]
        - splitters = MPI_Bcast(splitters, root=0) - Broadcasting splitters to all processes


        6. Partition elements into buckets based on splitters
        - Initialize local_buckets = [] * num_buckets
        - for each a in local_segment:
            - Find j such that splitters[j-1] < a <= splitters[j]
            - Place a in local_buckets[j]


        7. Redistribute buckets among processes
        - Prepare 'send_counts' and 'send_displacements' for 'local_buckets'
        - Use MPI_Alltoallv to exchange elements such that each process receives a global bucket
        - Each process now holds one bucket (global_bucket) which contains all elements within a specific range


        8. Sort each bucket locally
        - Sort global_bucket using a appropriate sorting algorithm 


        9. Gather sorted buckets at the root process
        - sorted_buckets_all = MPI_Gather(global_bucket, root=0)


        10. Root process concatenates sorted buckets to form the final sorted array
        - if rank == 0:
            - final_sorted_array = concatenate(sorted_buckets_all)
            - return final_sorted_array


        11. Finalize MPI environment
        - MPI_Finalize()
```

### 2c. Evaluation plan - what and how will you measure and compare

- **Input sizes, Input types**

We'll test the algorithm with various input sizes to evaluate its performance on small and large datasets. The input types will include random integers, sorted arrays, and reverse-sorted arrays to see how the algorithm handles different data scenarios. Various input sizes would include powers of two which would be 2^10, 2^14, .... , 2^26 to understand the scalability of the algorithms.

- **Strong scaling**

We'll measure how the execution time changes when we increase the number of processors while keeping the problem size the same. This will show us how efficiently the algorithm scales with more processors. MPI processes for the identical inputs would include 2, 4, 8, 16, 32, 64, 128, 256, ... , 1024 to measure execution times. While doing this we would also test various inputs sizes as mentioned above.

- **Weak scaling**

We'll assess performance by increasing both the problem size and the number of processors proportionally. This will help us understand how well the algorithm maintains efficiency as workload and resources grow. Also, this would help us see how the algorithms handle large data.
