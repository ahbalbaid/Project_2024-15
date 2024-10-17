#include <iostream>
#include <vector>
#include <algorithm>
#include <mpi.h>
#include <cstdlib>
#include <caliper/cali.h>
#include <caliper/cali-manager.h>
#include <climits>

int* SampleSort(int n, int* elmnts, int* nsorted, MPI_Comm comm) {
    int i, j, nlocal, npes, myrank;
    int* sorted_elmnts;
    int* splitters = nullptr;
    int* allpicks = nullptr;
    int* scounts = nullptr;
    int* sdispls = nullptr;
    int* rcounts = nullptr;
    int* rdispls = nullptr;

    /* Get communicator-related information */
    MPI_Comm_size(comm, &npes);
    MPI_Comm_rank(comm, &myrank);
    nlocal = n / npes;



    /* Allocate memory for the arrays that will store the splitters */
    splitters = new int[npes];
    allpicks = new int[npes * (npes - 1)];

    /* Sort local array using std::sort */
    

    CALI_MARK_BEGIN("comp_small");
    std::sort(elmnts, elmnts + nlocal);
    CALI_MARK_END("comp_small");


    /* Select local npes-1 equally spaced elements */
    for (i = 1; i < npes; i++)
        splitters[i - 1] = elmnts[i * nlocal / npes];

    /* Gather the samples in the processors */
    CALI_MARK_BEGIN("comp_large");
    MPI_Allgather(splitters, npes - 1, MPI_INT, allpicks, npes - 1, MPI_INT, comm);
    CALI_MARK_END("comp_large");


    /* Sort these samples using std::sort */

    CALI_MARK_BEGIN("comp_small");
    std::sort(allpicks, allpicks + npes * (npes - 1));
    CALI_MARK_END("comp_small");    

    CALI_MARK_BEGIN("commm"); 
    CALI_MARK_BEGIN("comm_small");
    /* Select splitters */
    for (i = 1; i < npes; i++)
        splitters[i - 1] = allpicks[i * npes];
    splitters[npes - 1] = INT_MAX;
    CALI_MARK_END("comm_small");
    CALI_MARK_END("commm"); 

    /* comm lsrge the number of elements that belong to each bucket */
    CALI_MARK_BEGIN("commm"); 
    CALI_MARK_BEGIN("comm_large");
    scounts = new int[npes]();
    for (j = i = 0; i < nlocal; i++) {
        while (j < npes - 1 && elmnts[i] >= splitters[j]) {
            j++;
        }
        scounts[j]++;
    }
    CALI_MARK_END("comm_large");
    CALI_MARK_END("commm"); 



    /* Determine the starting location of each bucket's elements in the elmnts array */
    
    sdispls = new int[npes]();
    for (i = 1; i < npes; i++)
        sdispls[i] = sdispls[i - 1] + scounts[i - 1];

    /* Perform an all-to-all to inform the corresponding processes of the number of elements */
    rcounts = new int[npes];
    CALI_MARK_BEGIN("commm"); 
    CALI_MARK_BEGIN("comm_large");
    MPI_Alltoall(scounts, 1, MPI_INT, rcounts, 1, MPI_INT, comm);
    CALI_MARK_END("comm_small");
    CALI_MARK_END("commm"); 

    /* Based on rcounts determine where in the local array the data from each processor */
    /* will be stored. This array will store the received elements as well as the final */
    /* sorted sequence */
    rdispls = new int[npes]();
    for (i = 1; i < npes; i++)
        rdispls[i] = rdispls[i - 1] + rcounts[i - 1];
    *nsorted = rdispls[npes - 1] + rcounts[npes - 1];
    sorted_elmnts = new int[*nsorted];

    /* Each process sends and receives the corresponding elements */
    CALI_MARK_BEGIN("commm"); 
    MPI_Alltoallv(elmnts, scounts, sdispls, MPI_INT, sorted_elmnts, rcounts, rdispls, MPI_INT, comm);
    CALI_MARK_END("commm"); 


    /* Perform the final local sort using std::sort */

 
    CALI_MARK_BEGIN("comp_small"); 
    std::sort(sorted_elmnts, sorted_elmnts + (*nsorted));
    CALI_MARK_END("comp_small");


    /* Free allocated memory */
    delete[] splitters;
    delete[] allpicks;
    delete[] scounts;
    delete[] sdispls;
    delete[] rcounts;
    delete[] rdispls;

    return sorted_elmnts;
}

int main(int argc, char* argv[]) {
    CALI_CXX_MARK_FUNCTION;
    int n;
    int npes;
    int myrank;
    int nlocal;
    int* elmnts;  /* array that stores the local elements */
    int* vsorted; /* array that stores the final sorted elements */
    int nsorted;  /* number of elements in vsorted */
    double stime, etime;

    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &npes);
    MPI_Comm_rank(MPI_COMM_WORLD, &myrank);

    cali::ConfigManager mgr;
    mgr.start();

    if (argc != 2) {
        if (myrank == 0) {
            std::cout << "Usage: mpiexec -n <p> " << argv[0] << " <n>" << std::endl;
        }
        //MPI_Finalize();
        return 1;
    }

    n = atoi(argv[1]);
    nlocal = n / npes; /* Compute the number of elements to be stored locally. */

    /* Allocate memory for the various arrays */
    elmnts = new int[nlocal];

    /* Fill-in the elmnts array with random elements */
    //

    CALI_MARK_BEGIN("data_init_runtime");
    srand(myrank);


    for (int i = 0; i < nlocal; i++) {
        elmnts[i] = rand() % (10 * n + 1);
    }
    CALI_MARK_END("data_init_runtime");
    
    CALI_MARK_BEGIN("comp");
    MPI_Barrier(MPI_COMM_WORLD);

    stime = MPI_Wtime();

    // comp start
    
    vsorted = SampleSort(n, elmnts, &nsorted, MPI_COMM_WORLD);
    CALI_MARK_END("comp");
    //comp end
    etime = MPI_Wtime();


    CALI_MARK_BEGIN("MPI_Barrier");
    MPI_Barrier(MPI_COMM_WORLD);
    CALI_MARK_END("MPI_Barrier");

    /* Gather size of sorted arrays from all processes */
    int total_sorted_elements = nsorted;
    int* total_counts = new int[npes];
    //comm large start
    CALI_MARK_BEGIN("comm-large");
    MPI_Gather(&nsorted, 1, MPI_INT, total_counts, 1, MPI_INT, 0, MPI_COMM_WORLD);
    CALI_MARK_BEGIN("comm-large");
    //comm large end

    CALI_MARK_BEGIN("correctness_check");
    if (myrank == 0) {
        for (int i = 1; i < npes; i++) {
            total_sorted_elements += total_counts[i];
        }
        std::cout << "Total sorted elements: " << total_sorted_elements << std::endl;
        std::cout << "Expected sorted elements: " << n << std::endl;

        // Check if the sorted array is valid
        bool is_sorted = std::is_sorted(vsorted, vsorted + nsorted);
        std::cout << "Is the sorted array valid? " << (is_sorted ? "Yes" : "No") << std::endl;
        std::cout << "Sorting time: " << etime - stime << " sec" << std::endl;
    }
    CALI_MARK_END("correctness_check");

    delete[] elmnts;
    delete[] vsorted;
    delete[] total_counts;

    mgr.stop();
    mgr.flush();

    MPI_Finalize();

    return 0;
}
