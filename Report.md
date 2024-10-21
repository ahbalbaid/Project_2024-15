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
The array to be sorted is divided among multiple cores, and each core is assigned a sub-array to perform the merge sort in parallel. The process begins with the master core distributing sub-arrays to each core via MPI_Scatter. Each core independently sorts its assigned sub-array using merge sort. After sorting, the sub-arrays are gathered back to the master core using MPI_Gather, where a final merge step combines the sorted sub-arrays into a fully sorted array.

<img width="1018" alt="Screenshot 2024-10-16 at 9 58 50 PM" src="https://github.com/user-attachments/assets/d074d70f-f20e-4c63-90ee-c22beb124979">

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
![Screenshot 2024-10-16 221318](https://github.com/user-attachments/assets/d4170c69-adbd-42f6-b5aa-89b834e6acef)

Radix Sort
```
ParallelRadixSort(arr):
    if length(arr) <= 1:
        return arr

    max_digit = number of bits in the largest number in arr
    
    // Master Processor
    for d = max_digit-1 down to 0 do:
        // Broadcast the current digit position `d` to all processors

        // Split the array among worker processors
        // Each processor computes which numbers have `0` at digit `d`
        places = Map each element in arr to True if bit `d` is 0, otherwise False

        // Initialize two lists, stack (for 0s) and remaining (for 1s)
        stack = empty list
        remaining = empty list

        // Each processor classifies its part of arr based on places
        for each index idx in places:
            if places[idx] == True:
                Add arr[idx] to stack
            else:
                Add arr[idx] to remaining
        
        // Combine results from all processors (gather stack and remaining)
        arr = stack concatenated with remaining

    return arr
```

This parallel radix sort algorithm sorts an array by processing it bit by bit, starting from the least significant bit. It uses multiple processors to speed up the sorting. First, it identifies the largest number in the array to determine the maximum number of bits needed for comparison. The master processor broadcasts the current bit position (digit) to all worker processors, which then divide the array for parallel processing.

Each processor checks its portion of the array, mapping whether each element has a 0 or 1 at the current bit. Numbers with a 0 are placed in one list (stack of zeros), while those with a 1 go into another (remaining of ones). After this classification, results from all processors are gathered, and the array is updated by combining the lists. This process is repeated for each bit until the array is sorted. The parallelization allows the algorithm to handle large datasets efficiently.

![Screenshot 2024-10-16 at 9 58 59 PM](https://github.com/user-attachments/assets/e2867ffa-2b97-47a5-8008-e3a87c9dd825)

Radix Sort github: https://github.com/naps62/parallel-sort/blob/master/src/radix.mpi.cpp
```
ParallelRadixSort(arr, id, num_processes, num_bits_per_pass):
    # Initialize constants
    num_buckets = 2^num_bits_per_pass  # total number of buckets
    buckets_per_processor = num_buckets / num_processes  # number of buckets each processor handles
    mask = (1 << num_bits_per_pass) - 1  # mask for extracting key bits

    # Initialize data structures for buckets and counts
    buckets = array of empty lists, size num_buckets
    bucket_counts = 2D array to track count of elements per bucket and processor
    bucket_accum = 2D array to track positions for placing elements in the final sorted array
    bucket_sizes = array for storing bucket sizes per processor

    # Main loop for sorting based on each set of bits
    while mask != 0:
        # Reset bucket counts and clear buckets
        Reset bucket counts and empty buckets

        # Distribute elements into buckets based on the current bits (using mask)
        for each element in arr:
            determine the appropriate bucket for the element
            add element to the bucket
            update bucket count

        # Share and combine bucket counts across all processes

        # Calculate positions (bucket_accum) for each element in the final sorted array

        # Send bucket elements to corresponding processors based on bucket allocation

        # Receive elements from other processors and place them in the correct positions

        # Update the array for the next pass
        arr = combined data from all buckets for this processor
        shift the mask to process the next set of bits

    return arr
```

<img width="1064" alt="Screenshot 2024-10-16 at 9 53 53 PM" src="https://github.com/user-attachments/assets/ab69a030-929a-4fe1-add7-38a9c48b28f0">

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
<img width="1010" alt="image" src="https://github.com/user-attachments/assets/ba218105-50df-4529-8d67-bb16f3087b5b">


### 2c. Evaluation plan - what and how will you measure and compare

- **Input sizes, Input types**

We'll test the algorithm with various input sizes to evaluate its performance on small and large datasets. The input types will include random integers, sorted arrays, and reverse-sorted arrays to see how the algorithm handles different data scenarios. Various input sizes would include powers of two which would be 2^16, 2^18, 2^20, 2^22, 2^24, 2^26, 2^28 to understand the scalability of the algorithms.

- **Strong scaling**

We'll measure how the execution time changes when we increase the number of processors while keeping the problem size the same. This will show us how efficiently the algorithm scales with more processors. MPI processes for the identical inputs would include 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024 to measure execution times. While doing this we would also test various inputs sizes as mentioned above.

- **Weak scaling**

We'll assess performance by increasing both the problem size and the number of processors proportionally. This will help us understand how well the algorithm maintains efficiency as workload and resources grow. Also, this would help us see how the algorithms handle large data.
