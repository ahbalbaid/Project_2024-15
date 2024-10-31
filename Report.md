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
// SampleSort Pseudocode using MPI and std::sort

Function SampleSort(n, elmnts, comm):
    Initialize npes, myrank, nlocal as number of processes, rank, and size of local data
    Initialize splitters, allpicks, scounts, sdispls, rcounts, rdispls as empty arrays

    // Get communicator information
    Call MPI_Comm_size(comm, npes)
    Call MPI_Comm_rank(comm, myrank)
    Set nlocal as size of elmnts array

    Resize splitters to size npes
    Resize allpicks to size npes * (npes - 1)

    // Sort local array
    Call std::sort(elmnts.begin(), elmnts.end())

    // Select npes-1 equally spaced local splitters
    For i from 1 to npes:
        Set splitters[i - 1] = elmnts[i * nlocal / npes]

    // Gather splitters from all processes
    Call MPI_Allgather(splitters.data(), npes - 1, MPI_INT, allpicks.data(), npes - 1, MPI_INT, comm)

    // Sort the gathered splitters
    Call std::sort(allpicks.begin(), allpicks.end())

    // Select global splitters from sorted allpicks
    For i from 1 to npes:
        Set splitters[i - 1] = allpicks[i * npes]
    Set splitters[npes - 1] to INT_MAX

    // Count elements for each bucket (scounts)
    Initialize scounts as array of size npes with 0
    Set j = 0
    For i from 0 to nlocal:
        If elmnts[i] < splitters[j]:
            Increment scounts[j]
        Else:
            Increment scounts[++j]

    // Calculate starting index for each bucket in local data (sdispls)
    Initialize sdispls as array of size npes with 0
    For i from 1 to npes:
        Set sdispls[i] = sdispls[i - 1] + scounts[i - 1]

    // Perform all-to-all to get receive counts (rcounts)
    Initialize rcounts as array of size npes
    Call MPI_Alltoall(scounts.data(), 1, MPI_INT, rcounts.data(), 1, MPI_INT, comm)

    // Calculate starting index for received elements (rdispls)
    Initialize rdispls as array of size npes with 0
    For i from 1 to npes:
        Set rdispls[i] = rdispls[i - 1] + rcounts[i - 1]

    // Calculate total elements to receive (nsorted) and resize sorted_elmnts array
    Set nsorted = rdispls[npes - 1] + rcounts[npes - 1]
    Initialize sorted_elmnts as array of size nsorted

    // Send and receive elements using MPI_Alltoallv
    Call MPI_Alltoallv(elmnts.data(), scounts.data(), sdispls.data(), MPI_INT,
                       sorted_elmnts.data(), rcounts.data(), rdispls.data(), MPI_INT, comm)

    // Perform final local sort on received data
    Call std::sort(sorted_elmnts.begin(), sorted_elmnts.end())

    Return sorted_elmnts
```
<img width="1010" alt="image" src="https://github.com/user-attachments/assets/ba218105-50df-4529-8d67-bb16f3087b5b">


### 2c. Evaluation plan - what and how will you measure and compare

- **Input sizes, Input types**

We'll test the algorithm with various input sizes to evaluate its performance on small and large datasets. The input types will include random integers, sorted arrays, and reverse-sorted arrays to see how the algorithm handles different data scenarios. Various input sizes would include powers of two which would be 2^16, 2^18, 2^20, 2^22, 2^24, 2^26, 2^28 to understand the scalability of the algorithms.

- **Strong scaling**

We'll measure how the execution time changes when we increase the number of processors while keeping the problem size the same. This will show us how efficiently the algorithm scales with more processors. MPI processes for the identical inputs would include 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024 to measure execution times. While doing this we would also test various inputs sizes as mentioned above.

- **Weak scaling**

We'll assess performance by increasing both the problem size and the number of processors proportionally. This will help us understand how well the algorithm maintains efficiency as workload and resources grow. Also, this would help us see how the algorithms handle large data.


## 3. Project Results

Please note that the results may not be fully comprehensive due to the long queue in Grace. However, we have made every effort to include as much data as possible to evaluate the performance.

### Merge Sort

#### **Strong scaling for each array size**
<img width="888" alt="Screenshot 2024-10-30 at 9 05 30 PM" src="https://github.com/user-attachments/assets/93393d1b-8abb-4ef9-9936-ebb461397692">
<img width="888" alt="Screenshot 2024-10-30 at 9 05 27 PM" src="https://github.com/user-attachments/assets/fd837a35-1b9a-4966-9044-44b28d42c3b0">
<img width="888" alt="Screenshot 2024-10-30 at 9 05 24 PM" src="https://github.com/user-attachments/assets/ec37af3b-81dc-45f9-a907-ff03b48f8df5">

<img width="888" alt="Screenshot 2024-10-30 at 9 06 22 PM" src="https://github.com/user-attachments/assets/acc6ce0b-6596-477b-9068-fe0ec5c51db0">
<img width="888" alt="Screenshot 2024-10-30 at 9 06 19 PM" src="https://github.com/user-attachments/assets/68157123-c9a9-4298-871a-b97d4e630236">
<img width="888" alt="Screenshot 2024-10-30 at 9 06 17 PM" src="https://github.com/user-attachments/assets/13620b1b-0ad0-4511-9f3a-eea576341e0c">

<img width="888" alt="Screenshot 2024-10-30 at 9 06 40 PM" src="https://github.com/user-attachments/assets/4699c24d-241a-46a3-b2bd-403921d17cba">
<img width="888" alt="Screenshot 2024-10-30 at 9 06 38 PM" src="https://github.com/user-attachments/assets/a562ddeb-823e-4194-83d2-897c23dfd52b">
<img width="888" alt="Screenshot 2024-10-30 at 9 06 36 PM" src="https://github.com/user-attachments/assets/eae5179e-2755-42cd-b77e-7fad568c0fa1">

<img width="888" alt="Screenshot 2024-10-30 at 9 07 02 PM" src="https://github.com/user-attachments/assets/aff208fc-9fe5-4151-b8a9-d71b48033d9a">
<img width="888" alt="Screenshot 2024-10-30 at 9 07 00 PM" src="https://github.com/user-attachments/assets/a37e45ad-a01d-451a-92cc-fecf91d1aa93">
<img width="888" alt="Screenshot 2024-10-30 at 9 06 58 PM" src="https://github.com/user-attachments/assets/209a0df1-d547-4b44-a3ed-6514cf2c2d9b">

<img width="888" alt="Screenshot 2024-10-30 at 9 07 17 PM" src="https://github.com/user-attachments/assets/3e476d36-28d1-426d-96e1-249b26e8135a">
<img width="888" alt="Screenshot 2024-10-30 at 9 07 16 PM" src="https://github.com/user-attachments/assets/27a9458f-5182-4116-aaba-a8e3853e6ffc">
<img width="888" alt="Screenshot 2024-10-30 at 9 07 13 PM" src="https://github.com/user-attachments/assets/b672fad3-ffa6-4a1e-9638-46142ad4a298">

<img width="888" alt="Screenshot 2024-10-30 at 9 07 39 PM" src="https://github.com/user-attachments/assets/9fd7106f-ad1e-4960-b7cf-c9a43922f32c">
<img width="888" alt="Screenshot 2024-10-30 at 9 07 37 PM" src="https://github.com/user-attachments/assets/3ed5290d-f5bf-49e2-a2d8-d79d765bb9c2">
<img width="888" alt="Screenshot 2024-10-30 at 9 07 35 PM" src="https://github.com/user-attachments/assets/023f203c-c416-4a43-a5bd-20791b4373b1">

<img width="888" alt="Screenshot 2024-10-30 at 9 07 58 PM" src="https://github.com/user-attachments/assets/ca9fb013-8f60-4209-8072-76837888852d">
<img width="888" alt="Screenshot 2024-10-30 at 9 07 56 PM" src="https://github.com/user-attachments/assets/a2e77f5b-11bf-4cac-823c-8d2beb813de5">
<img width="888" alt="Screenshot 2024-10-30 at 9 07 54 PM" src="https://github.com/user-attachments/assets/a3cbfa02-0f70-4869-b771-73dc5579042c">

In merge sort strong scaling graphs, we can see that larger amount of processors, which means distributing the work across more processors generally decreases the average computation time per processor as each processor handles a smaller portion of the total array. This leads to a reduction in the time required for each processor to sort its assigned sub-array, so the average time per rank initially decreases as you add processors.

However, as the array size becomes smaller, the benefit of adding more processors diminishes due to communication overhead. For large arrays, computation time (actual sorting work) dominates, making the communication overhead relatively minor. But with smaller array sizes, the computation per processor becomes minimal, so the time spent in communication and synchronization between processors such as calls to MPI_GATHER and MPI_SCATTER becomes a larger portion of the total runtime. At this point, communication overhead outweighs the gains from parallelism, limiting the effectiveness of adding more processors and causing the average time per rank to stay constant or even increase, as we can see from the graphs with lower array sizes.



#### **Strong scaling speedup - Random**
<img width="888" alt="speedup - random" src="https://github.com/user-attachments/assets/617c9f46-08aa-4983-8a07-b3fa2de71fc2">
<img width="888" alt="comm - speed" src="https://github.com/user-attachments/assets/3fa57a95-7b80-43d1-aa83-ff7f1b0d5ebf">
<img width="888" alt="comp - speed" src="https://github.com/user-attachments/assets/5fdac1a0-06ff-4b3e-b254-9f273a2aaed0">


In strong scaling graphs, we can see that the speedup increases significantly for larger array sizes because larger workloads have more computational work to distribute across processors. When the problem is small, the overhead from communication between the processors outweighs the benefits of adding more processors, leading to diminishing returns in speedup. However, with larger problem sizes, the actual computation dominates over this overhead, so each processor can perform a substantial amount of work without the overheads taking over a larger percentage of the total time. 

#### **Strong scaling speedup - Sorted**
<img width="888" alt="speedup - sorted" src="https://github.com/user-attachments/assets/7aea1169-1f9a-4ff9-b833-4b51d3c47c16">
<img width="888" alt="comm -sorted - speed" src="https://github.com/user-attachments/assets/7aeb7c42-206d-4b35-90ba-cc5c3921240b">
<img width="888" alt="comp - sorted - speed" src="https://github.com/user-attachments/assets/a6192305-0995-4cdb-9658-2bdacf030201">

Sorted strong scaling speedup graph follows the same logic as the random speedup. See above. 

#### **Strong scaling speedup - Reverse Sorted**
<img width="888" alt="speedup - reverse" src="https://github.com/user-attachments/assets/0a025faa-5097-4f9a-8bcb-2a0ee6b1c37a">
<img width="888" alt="comp - rev - speed" src="https://github.com/user-attachments/assets/ec7490d7-f23b-47be-92d0-4ce395f6a6f1">
<img width="888" alt="comm - rev - speed" src="https://github.com/user-attachments/assets/4e3bfaac-ab84-4fe0-a662-8dfe26c70de0">

Reverse sorted strong scaling speedup graph follows the same logic as the random speedup. See above. 

#### **Strong scaling speedup - 1% Perturbed**
<img width="888" alt="speedup - perturbed" src="https://github.com/user-attachments/assets/156ed98a-cdc5-40e9-856d-9a6372ec1f4b">
<img width="888" alt="comp - perturbed - speed" src="https://github.com/user-attachments/assets/57e6cef4-4282-4cae-95c0-e6655bcb527a">
<img width="888" alt="comm - pert - speed" src="https://github.com/user-attachments/assets/18018f2b-17ad-498d-9b65-e7162dd188d5">

Perturbed strong scaling speedup graph follows the same logic as the random speedup. See above. 

#### **Weak Scaling - Random**
<img width="888" alt="Screenshot 2024-10-30 at 8 58 19 PM" src="https://github.com/user-attachments/assets/e284d883-c7a7-434c-91d0-bdf126f7d685">
<img width="888" alt="Screenshot 2024-10-30 at 8 58 16 PM" src="https://github.com/user-attachments/assets/410ca918-56e4-4527-ae78-af92a3291eda">
<img width="888" alt="Screenshot 2024-10-30 at 8 58 13 PM" src="https://github.com/user-attachments/assets/3d1a2e42-4c3c-4d7b-878e-ab7a2b340192">

In the weak scaling graph for Random sort method, the main function time is roughly constant as both the array size and the number of processors increase proportionally. This constant is achieved because each processor is handling about the same amount of work as in the original setup. Apart from smaller array sizes where overhead overshadows the distribution of work in the total time, the constant main function time is expected apart from a few outliers.  

#### **Weak Scaling - Sorted**
<img width="888" alt="Screenshot 2024-10-30 at 8 58 49 PM" src="https://github.com/user-attachments/assets/97747d67-6b6e-4ba5-9859-0766426852f4">
<img width="888" alt="Screenshot 2024-10-30 at 8 58 46 PM" src="https://github.com/user-attachments/assets/c009edc1-77b4-4774-8a75-595407ada2c9">
<img width="888" alt="Screenshot 2024-10-30 at 8 58 43 PM" src="https://github.com/user-attachments/assets/08f22ef2-f6fe-4f98-9675-40ce6d777eed">

Sorted weak scaling graph follows the same logic as the random weak scaling. See above. 


#### **Weak Scaling - Reverse Sorted**
<img width="888" alt="Screenshot 2024-10-30 at 8 56 06 PM" src="https://github.com/user-attachments/assets/bceea094-b5a3-401f-a0a8-adb9c0249570">
<img width="888" alt="Screenshot 2024-10-30 at 8 56 03 PM" src="https://github.com/user-attachments/assets/cc0d6750-5787-421d-88c3-0e1520a5e074">
<img width="888" alt="Screenshot 2024-10-30 at 8 55 57 PM" src="https://github.com/user-attachments/assets/8eb975d5-6a8d-414c-a8ff-e36a211ade2f">

Reverse sorted weak scaling graph follows the same logic as the random weak scaling. See above. 


#### **Weak Scaling - Perturbed**
<img width="888" alt="Screenshot 2024-10-30 at 8 57 20 PM" src="https://github.com/user-attachments/assets/28233019-15d1-472a-8993-2d257168f94c">
<img width="888" alt="Screenshot 2024-10-30 at 8 57 18 PM" src="https://github.com/user-attachments/assets/b0004123-8bd0-44d2-8d49-52e63571b714">
<img width="888" alt="Screenshot 2024-10-30 at 8 57 15 PM" src="https://github.com/user-attachments/assets/ccd9a927-4a4d-4c52-99ba-b6f1e6f62af7">

Perturbed weak scaling graph follows the same logic as the random weak scaling. See above. 

### Bitonic Sort

#### **Weak scaling**
Bitonic is an unstable algorithm which means it reorders the elements in the array even if they are already sorted. The sorting time decreases for each input size and the largest array had the greatest speedup. Computation dominates the overall runtime which includes local sequential sort and merging. As parallelization increases, each thread operates on a smaller portion of data.  

**Main**:  
![image](https://github.com/user-attachments/assets/f08e75e4-08c3-4059-8660-cf43638324b2)
![image](https://github.com/user-attachments/assets/cf54d28a-fa91-4d8b-88bd-3c05e7aed336)
![image](https://github.com/user-attachments/assets/e4189bad-193b-4f8f-ba9f-6a7812502913)
![image](https://github.com/user-attachments/assets/cda57cf4-3bdb-42ed-b20c-f43a8ef0ada7)
**Comp_Large**:  
![image](https://github.com/user-attachments/assets/abfe73e6-ec89-46ff-aeca-220de7c53727)
![image](https://github.com/user-attachments/assets/e6b3928e-9796-449a-a66e-c6db9db7ed2e)
![image](https://github.com/user-attachments/assets/866f460b-5e7c-4a8e-b579-9b7a89b00384)
![image](https://github.com/user-attachments/assets/e8d006ff-886e-413e-a6f9-d6eee023edff)
**Comm**:  
![image](https://github.com/user-attachments/assets/552553d3-b12a-43db-ba68-3e344de25172)
![image](https://github.com/user-attachments/assets/31e45a85-f10a-4ec5-b7a6-f6b0dc4fe07f)
![image](https://github.com/user-attachments/assets/ba1145b1-6672-459f-a76f-55abad22aec1)
![image](https://github.com/user-attachments/assets/0c14fac8-6ee3-4cd1-affa-e30ddf7ef99b)

#### **Strong scaling speedup**
**Main**:  
![image](https://github.com/user-attachments/assets/010bc4cf-cbc3-4923-8de9-692c7fee63ad)
![image](https://github.com/user-attachments/assets/b8851929-1c85-42d7-9970-a102b857e2f7)
![image](https://github.com/user-attachments/assets/0179148d-b915-4067-9c62-95b20eebe8ef)
![image](https://github.com/user-attachments/assets/64d37430-d13c-4b2f-bd6b-54e88cd7b205)  
**Comp_Large**:  
![image](https://github.com/user-attachments/assets/354ff9ce-433e-4801-b879-639dfd32150c)
![image](https://github.com/user-attachments/assets/66b020a6-a761-40a4-b19e-5f04338919c4)
![image](https://github.com/user-attachments/assets/43fc5946-a86b-48f9-ad62-7d534d550c8e)
![image](https://github.com/user-attachments/assets/dc118d53-73fe-426c-ba53-3666b7d984d9)  
**Comm**:  
![image](https://github.com/user-attachments/assets/a8b44195-ae2f-4b4c-8aa6-78b5b6d1a38f)
![image](https://github.com/user-attachments/assets/8ee16786-2b8c-42d9-8ec2-f769de15fe74)
![image](https://github.com/user-attachments/assets/08cfc750-4ba2-4353-beca-6190ab3f4998)
![image](https://github.com/user-attachments/assets/074f9445-1bb3-434c-b4d1-923a905d9569)

#### **All Input Types, Strong scaling**
**Input size 2^24**:  
![image](https://github.com/user-attachments/assets/db0b45d2-af5b-487d-8e9d-e1ae04ab59b9)
![image](https://github.com/user-attachments/assets/6e2264cb-e154-44e9-b742-a66f69e4c88c)
![image](https://github.com/user-attachments/assets/4016b791-43f9-4163-bbd1-73e9722eeb34)  
**Input size 2^26**:  
![image](https://github.com/user-attachments/assets/f388da14-fcdd-463f-80e1-ffe4b73c53cb)
![image](https://github.com/user-attachments/assets/ce8e183f-6f23-4444-aefa-6950a0074418)
![image](https://github.com/user-attachments/assets/b2561413-5353-4ecb-9266-690b526d2c73)  
**Input size 2^28**:  
![image](https://github.com/user-attachments/assets/0c326091-5701-46e9-9a4b-041a05f76130)
![image](https://github.com/user-attachments/assets/2b6cd612-eec6-4f3d-8495-5dc6e66994c5)
![image](https://github.com/user-attachments/assets/052e76c9-ecc7-4e6c-acc1-a3e38bad384e)

#### **Random Input Arrays**
The random input arrays took the most amount of time compared to the different inputs. More time is needed on local sequential sort since not enough threads are available to utilize the parallel nature of bitonic sort. As the number of threads increase, each input size converges to a low runtime. Communication remains relatively constant and accounts for a small portion of the overall runtime.   
  


#### **1% Perturbed Arrays**
The 1% perturbed arrays show similar trends compared to the other inputs. As the parallelization increases each input type and array size converges to similar runtimes. Increasing the number of threads past a 2^9 will not improve runtime significantly since parallelization overhead will become dominant.  



### Radix Sort
We couldn't run tests with 1024 processors because Grace had a proxy error. Plus, the queue was really long, so we were only able to test with random input arrays, reversed arrays, and mostly sorted ones. To keep things simple, we focused on the most important plots and based our observations on those.

#### **Random Input Arrays**
The plots show that parallel radix sort works well, especially with larger arrays. But for smaller ones, communication slows things down.

##### **Array Creation**
- **Max**: The maximum time to create random arrays stayed about the same, no matter how many processors we used.

    ![Array Creation Max Log Scale](./radixsort/final_trial/random_array/plot/max/array_creation_log.png)
    

##### **Master Function**
  
- **Max**: The max execution time for the master function went down as we added more processors for bigger arrays, but smaller arrays showed some ups and downs.

    - **Log Scale**:

    ![Master Function Max Log Scale](./radixsort/final_trial/random_array/plot/max/master_function_log.png)

    - **Regular Scale**:

    ![Master Function Max](./radixsort/final_trial/random_array/plot/max/master_function.png)
  

##### **Sort Validation**

- **Max**: The time it took to validate the sorted arrays stayed consistent as the processor count went up.

    ![Sort Validation Max Log Scale](./radixsort/final_trial/random_array/plot/max/sort_validation_log.png)

##### **Whole Function**
The whole function performed better as more processors were added, particularly with bigger arrays.
  
- **Max**: The max execution time was highest with the biggest arrays and fewer processors, but after a certain point, adding more processors didn't help much.

    - **Log Scale**:

    ![Whole Function Max Log Scale](./radixsort/final_trial/random_array/plot/max/whole_function_log.png)

    - **Regular Scale**:

    ![Whole Function Max](./radixsort/final_trial/random_array/plot/max/whole_function.png)
  

##### **Worker Function**
The worker function, which does most of the distributed work, scaled reasonably well with more processors.
  
- **Max**: The time for worker function execution dropped noticeably with more processors for bigger arrays. Smaller arrays, though, didn’t benefit as much because of the communication overhead.

    - **Log Scale**:

    ![Worker Function Max Log Scale](./radixsort/final_trial/random_array/plot/max/worker_function_log.png)

    - **Regular Scale**:

    ![Worker Function Max](./radixsort/final_trial/random_array/plot/max/worker_function.png)
  
#### **Reversed Input Arrays**

##### **Array Creation**
- **Max**: Similar to random arrays, the time to create reversed arrays didn’t change much with more processors.
    
    ![Array Creation Max Log Scale](./radixsort/final_trial/reversed_array/plot/max/array_creation_log.png)
    

##### **Master Function**
  
- **Max**: The master function’s max time decreased as we added more processors, especially for large arrays, though there were some inconsistencies with smaller arrays.

    - **Log Scale**:

    ![Master Function Max Log Scale](./radixsort/final_trial/reversed_array/plot/max/master_function_log.png)

    - **Regular Scale**:

    ![Master Function Max](./radixsort/final_trial/reversed_array/plot/max/master_function.png)
  

##### **Sort Validation**

- **Max**: The validation time stayed flat as processor counts increased.

    ![Sort Validation Max Log Scale](./radixsort/final_trial/reversed_array/plot/max/sort_validation_log.png)

##### **Whole Function**
The whole function showed better performance with more processors, especially for large arrays.
  
- **Max**: We saw the longest execution times with the biggest arrays and fewer processors, with smaller improvements after a certain point.

    - **Log Scale**:

    ![Whole Function Max Log Scale](./radixsort/final_trial/reversed_array/plot/max/whole_function_log.png)

    - **Regular Scale**:

    ![Whole Function Max](./radixsort/final_trial/reversed_array/plot/max/whole_function.png)
  

##### **Worker Function**
The worker function scaled well with more processors.
  
- **Max**: Worker function execution times dropped a lot with larger arrays and more processors, but for smaller arrays, the improvement was limited due to communication delays.

    - **Log Scale**:

    ![Worker Function Max Log Scale](./radixsort/final_trial/reversed_array/plot/max/worker_function_log.png)

    - **Regular Scale**:

    ![Worker Function Max](./radixsort/final_trial/reversed_array/plot/max/worker_function.png)

#### **Sorted Input Arrays**
Since the algorithm doesn’t check if the array is already sorted, the performance was similar to the other input types.

##### **Array Creation**
- **Max**: The time for creating sorted arrays stayed constant as the processor count increased.
    
    ![Array Creation Max Log Scale](./radixsort/final_trial/sorted_array/plot/max/array_creation_log.png)
    

##### **Master Function**
  
- **Max**: For large arrays, the max time for the master function decreased as more processors were added, with some fluctuations for smaller arrays.

    - **Log Scale**:

    ![Master Function Max Log Scale](./radixsort/final_trial/sorted_array/plot/max/master_function_log.png)

    - **Regular Scale**:

    ![Master Function Max](./radixsort/final_trial/sorted_array/plot/max/master_function.png)
  

##### **Sort Validation**

- **Max**: Sort validation times remained flat as we added more processors.

    ![Sort Validation Max Log Scale](./radixsort/final_trial/sorted_array/plot/max/sort_validation_log.png)

##### **Whole Function**
Adding processors improved the whole function’s performance, particularly with larger arrays.
  
- **Max**: The largest arrays had the longest execution times with fewer processors, with only small improvements beyond a certain point.

    - **Log Scale**:

    ![Whole Function Max Log Scale](./radixsort/final_trial/sorted_array/plot/max/whole_function_log.png)

    - **Regular Scale**:

    ![Whole Function Max](./radixsort/final_trial/sorted_array/plot/max/whole_function.png)
  

##### **Worker Function**
The worker function scaled well with more processors.
  
- **Max**: Execution times decreased significantly with larger arrays and more processors, but smaller arrays didn’t benefit as much due to communication overhead.

    - **Log Scale**:

    ![Worker Function Max Log Scale](./radixsort/final_trial/sorted_array/plot/max/worker_function_log.png)

    - **Regular Scale**:

    ![Worker Function Max](./radixsort/final_trial/sorted_array/plot/max/worker_function.png)

#### **1% Perturbed Arrays**
TBD



### Sample Sort

This is a detailed analysis of computation performance and communication performance along with figures and explanations of the analysis.


Note*** When looking at min time and max times of the graphs for average time/rank we can see that the minimum time would provide us with the baseline for the fastest possible communication time and the maximum time would provide us with the highest time, which could highlight any bottlenecks. 



**Sorted**


Avg time/rank - main
![image](https://github.com/user-attachments/assets/9a11ea90-b077-472b-ae86-130141798f53)

We can see that the average time for the sample sort implementation for sorted arrays seems to go down for larger array sizes and go up for smaller array sizes. Time spent on tasks such as communication seems to be increasing the average time for smaller arrays and decreasing the average ‘main’ time for larger arrays. There is also not a great distribution of data with regards to smaller arrays as there is with larger arrays. Sorted arrays also seem to have less balanced input which could also slow down ‘main’ times.



Avg time/rank - comp
![image](https://github.com/user-attachments/assets/922adfbb-285f-4826-b363-e0dce72df712)

The average computation times for ‘main’ seem to be going down for each array size except for smaller arrays that increase processors. This is likely due to overhead that is associated with increasing processors while the array is small, which negatively impacts this algorithm. Data is not distributed as efficiently as it could be. There is more time being spent communicating rather than actually sorting. 



Total time, Sorted - comm
![image](https://github.com/user-attachments/assets/2e68f7b7-16d6-4c82-b64b-fd9d347c4db7)

As processors increase the communication time also increases. The more processors you have means that you generally have more communication going on between the processors.


**Random** 


Avg. time/rank - main
![image](https://github.com/user-attachments/assets/759f4f3e-feaa-4b48-832b-eaf6e0cb4bad)

The average ‘main’ times for random arrays tend to go down except for smaller arrays. As said previously smaller arrays have seemed to be spending more time communicating as processors increase rather than sorting. Showing that communication is more dominant over computation. 



Avg. time/rank - comp
![image](https://github.com/user-attachments/assets/e4ce1009-47c6-4ff1-90ba-745ec9533c71)

Average computation times are very similar to that of the sorted arrays. **explanation is given under sorted array graph



Total Time, Random - comm
![image](https://github.com/user-attachments/assets/bb0a6cd6-3a08-49ba-a684-5767eb488089)

Total communication times are very similar to that of sorted arrays.  **explanation is given under sorted array graph



**Reverse sorted**


Avg time/rank - main
![image](https://github.com/user-attachments/assets/63bb055b-fd54-4ffa-9dd4-b1b052deccd8)

Average ‘main’ times for reverse sorted are very similar to that of sorted arrays.  **explanation is given under sorted array graph



Avg time/rank - comp
![image](https://github.com/user-attachments/assets/632696ee-9bad-433d-ad82-9faefc64ce00)

Average ‘comp’ times for reverse sorted are very similar to that of sorted arrays.  **explanation is given under sorted input type ‘comp’ graph.



Total time, Reverse Sorted - comm
![image](https://github.com/user-attachments/assets/1ca5e3a7-97fa-4258-a98f-7e4ceec1baf2)

Total ‘comm’ times for reverse sorted input type is very similar to that of the sorted input type ‘comm’.  **explanation is given under sorted input type ‘comm’ graph.



**1% Perturbed**


Avg. time/rank - main
![image](https://github.com/user-attachments/assets/8be827d1-d4e5-41b4-87ba-3192163c3596)

Average ‘main’ times for 1% perturbed are very similar to that of random arrays.  **explanation is given under random input graph



Avg time/rank - comp
![image](https://github.com/user-attachments/assets/28bf289b-cd87-4759-9107-4171f5c00b4f)

Average ‘comp’ times for 1% perturbed are very similar to that of random arrays.  **explanation is given under random input type ‘comp’ graph.



Total time, 1% Perturbed - comm
![image](https://github.com/user-attachments/assets/311f82d9-621f-4aff-a5f6-33181a88d276)



Total communication times are very similar to that of sorted arrays.  **explanation is given under sorted array graph



**Something to note:**
Avg. time/rank, random - comm
![image](https://github.com/user-attachments/assets/4a255120-11dc-4142-bce6-33cfac079138)

These are the total times for average communication times for random input size. Communication seems to be going down for larger arrays and up for smaller arrays. It seems that since there are more processors for larger arrays there is a smaller portion of data that is being sorted which creates fast communication times since sorting is done fairly quickly. We see almost the opposite happen for smaller arrays. More communication is happening because there is more data.



Avg rank/time, 1% perturbed - comm
![image](https://github.com/user-attachments/assets/88c7d495-838d-4d1e-8b27-07c9c6773802)


Similar trend to that of the random input arrays. **explanation is above under the random input graph.



Avg rank/time, sorted - comm
![image](https://github.com/user-attachments/assets/fd3d2fad-0f41-484d-a38a-00edf71cd550)

In the sorted input type graph we see that instead of going down as we saw in the random input type it stays fairly constant. This could be due to the input type being less balanced in terms of the array itself for each processor. Leading to inefficient sorting for each processor.



Avg rank/time, reverse sorted - comm
![image](https://github.com/user-attachments/assets/b7a077e9-93f8-4653-8d5f-8f3001133ea0)

Similar trend to that of the sorted input array. **explanation is above under the sorted input graph.



**Presentation:**


Main - Speedup, Random
![image](https://github.com/user-attachments/assets/4d1c5532-108b-4d20-bec6-fe46398ecdc8)


As you can see there is an overhead for smaller arrays as you increase processors. Time spent on communication and managing parallel tasks take a large portion of time which limits speedup. As for larger arrays, we see a benefit in dividing the workload as you increase the number of processors. 



Main - Avg Time per Rank (Array size: 2^ 28)
![image](https://github.com/user-attachments/assets/16aa4134-c8c3-4c20-a6ce-99e41d9c02d5)

Average time for ‘main’ decreases on varying input types. Random and perturbed are the ones on the faster end. This is probably due to the input types being more balanced for each processor for random and perturbed. Leading to inefficient sorting for each processor. Another possibility is how the inputs are sorted. This could also lead to some overhead. 



Total Time, Random - comm
![image](https://github.com/user-attachments/assets/bb0a6cd6-3a08-49ba-a684-5767eb488089)

As the number of processors increases, the longer it takes for communication between each process. Since there are more processors there is more communication that is taking place. As processes increase at the end it starts to level off. The leveling off could potentially be due to MPI’s implementation with high processor counts. Meaning that MPI is able to handle these processors when there are too many. 
