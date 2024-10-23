#include <mpi.h>
#include <iostream>
#include <cstdlib>
#include <cstring>

#include <caliper/cali.h>
#include <caliper/cali-manager.h>
#include <adiak.hpp>

#define MASTER 0      /* taskid of first task */
#define FROM_MASTER 1 /* setting a message type */
#define FROM_WORKER 2 /* setting a message type */

const char *whole_function = "whole_function";
const char *array_creation = "array_creation";
const char *master_function = "master_function";
const char *worker_function = "worker_function";
const char *sort_validation = "sort_validation";

int array_is_sorted(int *arr, int n)
{

   for (int i = 1; i < n; i++)
   {
      if (arr[i - 1] > arr[i])
      {
         return 0;
      }
   }

   return 1;
}

int find_max_element(int *arr, int n)
{
   int max_element = arr[0];
   for (int i = 1; i < n; i++)
   {
      if (arr[i] > max_element)
         max_element = arr[i];
   }
   return max_element;
}

void radix_sort_master(int *arr, int n, int numtasks)
{
   int taskid = MASTER;
   int numworkers = numtasks - 1;

   int max_element = find_max_element(arr, n);
   int max_digit = sizeof(max_element) * 8;

   MPI_Bcast(&n, 1, MPI_INT, MASTER, MPI_COMM_WORLD);
   MPI_Bcast(&max_digit, 1, MPI_INT, MASTER, MPI_COMM_WORLD);

   int chunk_size = n / numworkers;
   int extra = n % numworkers;

   int *sendcounts = (int *)malloc(numtasks * sizeof(int));
   int *displs = (int *)malloc(numtasks * sizeof(int));

   sendcounts[MASTER] = 0;
   displs[MASTER] = 0;

   int offset = 0;
   for (int i = 1; i < numtasks; i++)
   {
      int elements = (i - 1 < extra) ? chunk_size + 1 : chunk_size;
      sendcounts[i] = elements;
      displs[i] = offset;
      offset += elements;
   }

   int *zero_counts = (int *)malloc(numtasks * sizeof(int));
   int *one_counts = (int *)malloc(numtasks * sizeof(int));

   for (int d = 0; d < max_digit; d++)
   {

      int recvcount = 0;
      int *local_arr = NULL;

      MPI_Scatterv(
          arr,
          sendcounts,
          displs,
          MPI_INT,
          local_arr,
          recvcount,
          MPI_INT,
          MASTER,
          MPI_COMM_WORLD);

      int zero_count_local = 0;
      int one_count_local = 0;

      MPI_Gather(&zero_count_local, 1, MPI_INT, zero_counts, 1, MPI_INT, MASTER, MPI_COMM_WORLD);
      MPI_Gather(&one_count_local, 1, MPI_INT, one_counts, 1, MPI_INT, MASTER, MPI_COMM_WORLD);

      int *zero_displs = (int *)malloc(numtasks * sizeof(int));
      int *one_displs = (int *)malloc(numtasks * sizeof(int));

      zero_displs[0] = 0;
      one_displs[0] = 0;
      for (int i = 1; i < numtasks; i++)
      {
         zero_displs[i] = zero_displs[i - 1] + zero_counts[i - 1];
         one_displs[i] = one_displs[i - 1] + one_counts[i - 1];
      }

      int total_zero_count = zero_displs[numtasks - 1] + zero_counts[numtasks - 1];
      int total_one_count = one_displs[numtasks - 1] + one_counts[numtasks - 1];

      int *all_zeros = (int *)malloc(total_zero_count * sizeof(int));
      int *all_ones = (int *)malloc(total_one_count * sizeof(int));

      MPI_Gatherv(
          NULL, 0, MPI_INT,
          all_zeros, zero_counts, zero_displs, MPI_INT,
          MASTER, MPI_COMM_WORLD);

      MPI_Gatherv(
          NULL, 0, MPI_INT,
          all_ones, one_counts, one_displs, MPI_INT,
          MASTER, MPI_COMM_WORLD);

      memcpy(arr, all_zeros, total_zero_count * sizeof(int));
      memcpy(&arr[total_zero_count], all_ones, total_one_count * sizeof(int));

      free(all_zeros);
      free(all_ones);
      free(zero_displs);
      free(one_displs);

      MPI_Barrier(MPI_COMM_WORLD);
   }

   free(sendcounts);
   free(displs);
   free(zero_counts);
   free(one_counts);
}

void radix_sort_worker()
{
   int numtasks, taskid;
   MPI_Comm_rank(MPI_COMM_WORLD, &taskid);
   MPI_Comm_size(MPI_COMM_WORLD, &numtasks);

   int n, max_digit;
   MPI_Bcast(&n, 1, MPI_INT, MASTER, MPI_COMM_WORLD);
   MPI_Bcast(&max_digit, 1, MPI_INT, MASTER, MPI_COMM_WORLD);

   int numworkers = numtasks - 1;
   int chunk_size = n / numworkers;
   int extra = n % numworkers;

   int recvcount = (taskid - 1 < extra) ? chunk_size + 1 : chunk_size;

   for (int d = 0; d < max_digit; d++)
   {
      int *local_arr = (int *)malloc(recvcount * sizeof(int));

      MPI_Scatterv(
          NULL,
          NULL,
          NULL,
          MPI_INT,
          local_arr,
          recvcount,
          MPI_INT,
          MASTER,
          MPI_COMM_WORLD);

      int *zeros = (int *)malloc(recvcount * sizeof(int));
      int *ones = (int *)malloc(recvcount * sizeof(int));

      int zero_count_local = 0;
      int one_count_local = 0;

      for (int i = 0; i < recvcount; i++)
      {
         if ((local_arr[i] >> d) & 1)
            ones[one_count_local++] = local_arr[i];
         else
            zeros[zero_count_local++] = local_arr[i];
      }

      free(local_arr);

      MPI_Gather(&zero_count_local, 1, MPI_INT, NULL, 0, MPI_INT, MASTER, MPI_COMM_WORLD);
      MPI_Gather(&one_count_local, 1, MPI_INT, NULL, 0, MPI_INT, MASTER, MPI_COMM_WORLD);

      MPI_Gatherv(
          zeros, zero_count_local, MPI_INT,
          NULL, NULL, NULL, MPI_INT,
          MASTER, MPI_COMM_WORLD);
      MPI_Gatherv(
          ones, one_count_local, MPI_INT,
          NULL, NULL, NULL, MPI_INT,
          MASTER, MPI_COMM_WORLD);

      free(zeros);
      free(ones);

      MPI_Barrier(MPI_COMM_WORLD);
   }
}

void random_array(int *array, int array_size)
{
   for (int i = 0; i < array_size; i++)
   {
      array[i] = rand() % 100000;
   }
}

void sorted_array(int *array, int array_size)
{
   for (int i = 0; i < array_size; i++)
   {
      array[i] = i;
   }
}

void reverse_array(int *array, int array_size)
{
   for (int i = 0; i < array_size; i++)
   {
      array[i] = array_size - i - 1;
   }
}

void perturbed_array(int *array, int array_size)
{
   for (int i = 0; i < array_size; i++)
   {
      array[i] = i;
   }

   for (int i = 0; i < array_size / 100; i++)
   {
      array[rand() % array_size] = rand() % 100000;
   }
}

int main(int argc, char *argv[])
{
   CALI_CXX_MARK_FUNCTION;

   int array_size;
   int input_sort = 0;

   if (argc >= 2)
   {
      array_size = std::atoi(argv[1]);

      if (argc == 3)
      {
         input_sort = std::atoi(argv[2]);
      }
   }
   else
   {
      printf("Please provide the size of the array\n");
      return -1;
   }

   int numtasks, taskid;
   int *arr = NULL;

   MPI_Init(&argc, &argv);
   MPI_Comm_rank(MPI_COMM_WORLD, &taskid);
   MPI_Comm_size(MPI_COMM_WORLD, &numtasks);

   adiak::init(NULL);
   adiak::user();
   adiak::launchdate();
   adiak::libraries();
   adiak::cmdline();
   adiak::clustername();
   adiak::value("num_procs", numtasks);
   adiak::value("array_size", array_size);
   adiak::value("program_name", "mpi_radix_sort");
   adiak::value("array_datatype_size", sizeof(int));

   cali::ConfigManager mgr;
   mgr.start();

   CALI_MARK_BEGIN(whole_function);

   if (taskid == MASTER)
   {
      CALI_MARK_BEGIN(array_creation);

      arr = (int *)malloc(array_size * sizeof(int));
      std::srand(time(NULL));

      switch (input_sort)
      {
      case 0:
         random_array(arr, array_size);
         break;
      case 1:
         printf("Sorted_array.\n");
         sorted_array(arr, array_size);
         break;
      case 2:
         printf("Reverse_array.\n");
         reverse_array(arr, array_size);
         break;
      case 3:
         printf("Perturbed_array.\n");
         perturbed_array(arr, array_size);
         break;
      default:
         printf("Invalid input sort type. Defaulting to 'random'.\n");
         random_array(arr, array_size);
         break;
      }
      CALI_MARK_END(array_creation);

      CALI_MARK_BEGIN(master_function);

      radix_sort_master(arr, array_size, numtasks);

      CALI_MARK_END(master_function);

      CALI_MARK_BEGIN(sort_validation);

      if (array_is_sorted(arr, array_size))
         printf("TEST: PASSED\n");
      else
         printf("TEST: FAILED\n");

      CALI_MARK_END(sort_validation);

      free(arr);
   }
   else
   {
      CALI_MARK_BEGIN(worker_function);
      radix_sort_worker();
      CALI_MARK_END(worker_function);
   }

   CALI_MARK_END(whole_function);

   MPI_Barrier(MPI_COMM_WORLD);

   mgr.stop();
   mgr.flush();

   MPI_Finalize();
   return 0;
}
