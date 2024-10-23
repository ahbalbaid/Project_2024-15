#include "mpi.h"
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <time.h>
#include <math.h>

#include <caliper/cali.h>
#include <caliper/cali-manager.h>
#include <adiak.hpp>

int compare(const void* a, const void* b){
	const int* x = (int*) a;
	const int* y = (int*) b;

	if (*x > *y){
		return 1;
  }
	else if (*x < *y){
		return -1;
  }
	return 0;
}

int main (int argc, char *argv[]){
  CALI_CXX_MARK_FUNCTION;

  /* Define Caliper region names */
  const char* main = "main";
  const char* data_init_runtime = "data_init_runtime";
  const char* correctness_check = "correctness_check";
  const char* comm = "comm";
  const char* comm_large = "comm_large";
  const char* comp = "comp";    
  const char* comp_large = "comp_large";   
  const char* comm_small = "comm_small";
 
  cali::ConfigManager mgr;
  mgr.start();

  CALI_MARK_BEGIN(main);
      
  int power,
  sizeOfArray,
  inputType,
  numtasks,      
	taskid,             
  rc,
  i, j, k, l,
  d,
  prev;     

  if (argc == 3){
    power = atoi(argv[1]);
    sizeOfArray = pow(2, power);
    inputType = atoi(argv[2]);
  }
  else{
    printf("\n Please provide the size of the array and input type");
    return 0;
  }      


  MPI_Status status;
    
  MPI_Init(&argc,&argv);
  MPI_Comm_rank(MPI_COMM_WORLD,&taskid);
  MPI_Comm_size(MPI_COMM_WORLD,&numtasks);
  
  if (numtasks < 2 ) {
    printf("Need at least two MPI tasks. Quitting...\n");
    MPI_Abort(MPI_COMM_WORLD, rc);
    exit(1);
  }

  if (taskid == 0){
    printf("Input Size: 2^%d %d Type: %d Processes: %d\n", power, sizeOfArray, inputType, numtasks);
  }
  
  CALI_MARK_BEGIN(data_init_runtime);
  int localSize = sizeOfArray / numtasks;
  int localArray[localSize];

  switch(inputType){
    case 1: // sorted
      for (i = 0; i < localSize; i++){
        localArray[i] = i + taskid * localSize;
      }
      break;
      
    case 2: // reverse sorted
      for (i = 0; i < localSize; i++){
        localArray[i] = localSize - i + (numtasks - taskid - 1) * localSize;
      }
      break;
      
    case 3: // random
      srand((unsigned)time(NULL) + taskid);
      for (i = 0; i < localSize; i++){
        localArray[i] = rand();
      }
      break;
      
    case 4: // 1%
      for (i = 0; i < localSize; i++){
        localArray[i] = i + taskid * localSize;
      }

      for (i = 0; i < localSize * 0.01; i++){
        int index = rand() % localSize;
        localArray[index] = rand();
      }
      break;
    
    default:
      printf("Need input type. 1=Sorted, 2=Reverse, 3=Random, 4=1%Perturbed");
      return 0;
  }
  CALI_MARK_END(data_init_runtime);
 
  CALI_MARK_BEGIN(comp);
  CALI_MARK_BEGIN(comp_large);
  qsort(localArray, localSize, sizeof(int), compare);
  CALI_MARK_END(comp_large);
  CALI_MARK_END(comp);

  MPI_Barrier(MPI_COMM_WORLD);

  CALI_MARK_BEGIN(comp);
  CALI_MARK_BEGIN(comp_large);
  d = log2(numtasks);
  for (i = 0; i < d; i++){
    for (j = i; j >= 0; j--){

      int iBit = (taskid >> (i + 1)) & 1;
      int jBit = (taskid >> j) & 1;
      int partner[localSize];
      int copy[localSize];

      CALI_MARK_BEGIN(comm);
      CALI_MARK_BEGIN(comm_large);
      MPI_Send(&localArray, localSize, MPI_INT, taskid ^ (1 << j), 0, MPI_COMM_WORLD);
      MPI_Recv(&partner, localSize, MPI_INT, taskid ^ (1 << j), 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
      CALI_MARK_END(comm_large);
      CALI_MARK_END(comm);


      for (l = 0; l < localSize; l++)
      {
        copy[l] = localArray[l];
      }

      // merge smaller elements
      if (iBit == jBit){
        l = 0;
        k = 0;
        while (l + k < localSize){
          if (copy[l] > partner[k]){
            localArray[l + k] = partner[k];
            k++;
          }
          else{
            localArray[l + k] = copy[l];
            l++;
          }
        }
      }
      // merge larger elements
      else{
        l = localSize;
        k = localSize;
        while (l + k > localSize){
          if (copy[l - 1] < partner[k - 1]){
            localArray[l + k - localSize - 1] = partner[k - 1];
            k--;
          }
          else{
            localArray[l + k - localSize - 1] = copy[l - 1];
            l--;
          }
        } 
      }
    }
  }
  CALI_MARK_END(comp_large);
  CALI_MARK_END(comp);


  MPI_Barrier(MPI_COMM_WORLD);

  // for (i = 0; i < localSize; i++){
  //   printf("%d ", localArray[i]);
  // }
  // printf("%d\n", taskid);

  CALI_MARK_BEGIN(correctness_check);

  bool localSorted = true;
  bool globalSorted = false;

  for (i = 1; i < localSize; i++){
    if (localArray[i - 1] > localArray[i]){
      localSorted = false;
    }
  }

  if (taskid < numtasks - 1){
    MPI_Send(&localArray[localSize - 1], 1, MPI_INT, taskid + 1, 0, MPI_COMM_WORLD);
  }

  if (taskid > 0){
    MPI_Recv(&prev, 1, MPI_INT, taskid - 1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    if (prev > localArray[0]){
      localSorted = false;
    }
  }

  CALI_MARK_END(correctness_check);
  CALI_MARK_END(main);

  MPI_Reduce(&localSorted, &globalSorted, 1, MPI_C_BOOL, MPI_LAND, 0, MPI_COMM_WORLD);
  if (taskid == 0){
    if (globalSorted) {
      printf("Sorted\n");
    } 
    else {
      printf("Not Sorted\n");
    }
  }

  const char* input_type;
  switch(inputType){
    case 1:
      input_type = "Sorted";
      break;
    case 2:
      input_type = "Reverse Sorted";
      break;
    case 3:
      input_type = "Random";
      break;
    case 4:
      input_type = "1_perc_perturbed";
      break;
  }

  adiak::init(NULL);
  adiak::launchdate();    // launch date of the job
  adiak::libraries();     // Libraries used
  adiak::cmdline();       // Command line used to launch the job
  adiak::clustername();   // Name of the cluster
  adiak::value("algorithm", "bitonic"); // The name of the algorithm you are using (e.g., "merge", "bitonic")
  adiak::value("programming_model", "mpi"); // e.g. "mpi"
  adiak::value("data_type", "int"); // The datatype of input elements (e.g., double, int, float)
  adiak::value("size_of_data_type", sizeof(int)); // sizeof(datatype) of input elements in bytes (e.g., 1, 2, 4)
  adiak::value("input_size", sizeOfArray); // The number of elements in input dataset (1000)
  adiak::value("input_type", input_type); // For sorting, this would be choices: ("Sorted", "ReverseSorted", "Random", "1_perc_perturbed")
  adiak::value("num_procs", numtasks); // The number of processors (MPI ranks)
  adiak::value("scalability", "strong"); // The scalability of your algorithm. choices: ("strong", "weak")
  adiak::value("group_num", 15); // The number of your group (integer, e.g., 1, 10)
  adiak::value("implementation_source", "online"); // Where you got the source code of your algorithm. choices: ("online", "ai", "handwritten").
  
  MPI_Finalize();
  return 0;
}
