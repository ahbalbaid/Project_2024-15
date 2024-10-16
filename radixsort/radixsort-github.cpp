// This implementation is inspired by https://github.com/naps62/parallel-sort/blob/master/src/radix.mpi.cpp
#include "common_github.h"
#include <stdlib.h>
#include <vector>
#include <limits>
#include <iostream>
#include <string>
#include <sstream>
#include <mpi.h>
#include <cstring>
#include <caliper/cali.h>
#include <caliper/cali-manager.h>
#include <adiak.hpp>
using namespace std;

const char *whole_function = "whole_function";
const char *array_creation = "array_creation";
const char *sort_validation = "sort_validation";

#define GET_BUCKET_NUM(elem, mask, g, i) (((elem) & (mask)) >> ((g) * (i)))

enum OrderCodes
{
   ORDER_CORRECT = 0,
   ORDER_ERROR_BORDERS = -1,
   ORDER_ONLY_MASTER = -2,
};

enum Tags
{
   TAG_KEY_SEND = 1,
   TAG_CHECK_ORDER = 2,
   TAG_COUT_SIZE = 3,
   TAG_COUT_DATA = 4,
   TAG_BUCKET_COUNT = 5,
};

void test_data(vector<unsigned int> *arr, int id, int size)
{
   srand(time(NULL) * (id + 1));
   for (unsigned int i = 0; i < arr->size(); ++i)
      (*arr)[i] = rand() % 100000;
}

unsigned int popcount(unsigned int x)
{
   unsigned int c;
   for (c = 0; x; ++c)
      x &= x - 1;
   return c;
}

#define BUCKET_TO_CPU(bucket) (((bucket) & (~((bpp) - 1))) >> bpp_bc)
#define BUCKET_IN_CPU(bucket) ((bucket) & ((bpp) - 1))

void radix_mpi(vector<unsigned int> *&arr, const unsigned int id, const unsigned int p, const unsigned int g)
{
   const unsigned int b = (1 << g);
   const unsigned int bpp = b / p;
   const unsigned int bpp_bc = popcount(bpp - 1);

   unsigned int mask = ((1 << g) - 1);

   vector<vector<unsigned int>> buckets(b);
   vector<vector<unsigned int>> bucket_counts;
   for (unsigned int i = 0; i < b; ++i)
      bucket_counts.push_back(vector<unsigned int>(p));

   vector<unsigned int> bucket_counts_aux(b);

   vector<vector<unsigned int>> bucket_accum;
   for (unsigned int i = 0; i < bpp; ++i)
      bucket_accum.push_back(vector<unsigned int>(p));

   vector<unsigned int> bucket_sizes(bpp);
   vector<unsigned int> *this_bucket;

   MPI_Request request;
   MPI_Status status;

   for (unsigned int round = 0; mask != 0; mask <<= g, ++round)
   {
      for (unsigned int i = 0; i < b; ++i)
      {
         bucket_counts_aux[i] = 0;
         bucket_counts[i][id] = 0;
         buckets[i].clear();
      }

      for (unsigned int i = 0; i < arr->size(); ++i)
      {
         unsigned int elem = (*arr)[i];
         unsigned int bucket = GET_BUCKET_NUM(elem, mask, g, round);
         bucket_counts_aux[bucket]++;
         bucket_counts[bucket][id]++;
         buckets[bucket].push_back(elem);
      }

      for (unsigned int i = 0; i < p; ++i)
      {
         if (i != id)
            MPI_Isend(&bucket_counts_aux[0], b, MPI_INT, i, TAG_BUCKET_COUNT, MPI_COMM_WORLD, &request);
      }
      for (unsigned int i = 0; i < p; ++i)
      {
         if (i != id)
         {
            MPI_Recv(&bucket_counts_aux[0], b, MPI_INT, i, TAG_BUCKET_COUNT, MPI_COMM_WORLD, &status);
            for (unsigned int k = 0; k < b; ++k)
               bucket_counts[k][i] = bucket_counts_aux[k];
         }
      }

      int total_bucket_size = 0;
      for (unsigned int i = 0; i < bpp; ++i)
      {
         int single_bucket_size = 0;
         int global_bucket = i + id * bpp;

         for (unsigned int j = 0; j < p; ++j)
         {
            bucket_accum[i][j] = total_bucket_size;
            single_bucket_size += bucket_counts[global_bucket][j];
            total_bucket_size += bucket_counts[global_bucket][j];
         }
         bucket_sizes[i] = single_bucket_size;
      }

      this_bucket = new vector<unsigned int>(total_bucket_size);

      for (unsigned int i = 0; i < b; ++i)
      {
         unsigned int dest = BUCKET_TO_CPU(i);
         unsigned int local_bucket = BUCKET_IN_CPU(i);
         if (dest != id && buckets[i].size() > 0)
         {
            MPI_Isend(&(buckets[i][0]), buckets[i].size(), MPI_INT, dest, local_bucket, MPI_COMM_WORLD, &request);
         }
      }

      for (unsigned int b = 0; b < bpp; ++b)
      {
         unsigned int global_bucket = b + id * bpp;

         for (unsigned int i = 0; i < p; ++i)
         {
            unsigned int bucket_size = bucket_counts[global_bucket][i];

            if (bucket_size > 0)
            {
               unsigned int *dest = &(*this_bucket)[bucket_accum[b][i]];
               if (i == id)
               {
                  memcpy(dest, &(buckets[global_bucket][0]), bucket_size * sizeof(int));
               }
               else
               {
                  MPI_Recv(dest, bucket_size, MPI_INT, i, b, MPI_COMM_WORLD, &status);
               }
            }
         }
      }

      delete arr;
      arr = this_bucket;
   }
}

int check_array_order(vector<unsigned int> *&arr, unsigned int id, unsigned int size)
{
   for (unsigned int i = 1; i < arr->size(); ++i)
      if ((*arr)[i - 1] > (*arr)[i])
         return i;

   int is_ordered = 1, reduce_val;
   unsigned int next_val;
   MPI_Request request;
   MPI_Status status;

   if (id > 0)
   {
      int val_to_send = (arr->size() == 0) ? numeric_limits<int>::max() : (*arr)[0];
      MPI_Isend(&val_to_send, 1, MPI_INT, id - 1, TAG_CHECK_ORDER, MPI_COMM_WORLD, &request);
   }

   if (id < (unsigned int)size - 1)
   {
      MPI_Recv(&next_val, 1, MPI_INT, id + 1, TAG_CHECK_ORDER, MPI_COMM_WORLD, &status);
      is_ordered = (arr->size() == 0 || arr->back() <= next_val);
   }

   MPI_Reduce(&is_ordered, &reduce_val, 1, MPI_INT, MPI_LAND, 0, MPI_COMM_WORLD);

   if (id == 0)
   {
      if (reduce_val)
         return ORDER_CORRECT;
      else
         return ORDER_ERROR_BORDERS;
   }

   return ORDER_ONLY_MASTER;
}

#define MSG_SIZE 100

void ordered_print(char *str, unsigned int id, unsigned int size)
{
   if (id == 0)
   {
      cout << str;
      MPI_Status status;
      for (unsigned int i = 1; i < size; ++i)
      {
         char buff[MSG_SIZE];
         MPI_Recv(buff, MSG_SIZE, MPI_BYTE, i, TAG_COUT_DATA, MPI_COMM_WORLD, &status);
         cout << buff;
      }
   }
   else
   {
      MPI_Send(str, MSG_SIZE, MPI_BYTE, 0, TAG_COUT_DATA, MPI_COMM_WORLD);
   }
}

int main(int argc, char **argv)
{
   cali::ConfigManager mgr;
   mgr.start();

   CALI_MARK_BEGIN(whole_function);

   adiak::init(NULL);
   adiak::user();
   adiak::launchdate();
   adiak::libraries();
   adiak::cmdline();
   adiak::clustername();

   int g = 4;
   int id, size;
   int len;

   MPI_Init(&argc, &argv);
   MPI_Comm_rank(MPI_COMM_WORLD, &id);
   MPI_Comm_size(MPI_COMM_WORLD, &size);

   if (argc > 1)
      len = atoi(argv[1]);
   else
      len = LEN;

   if (argc > 2)
      g = atoi(argv[2]);

   if (id == 0)
      cerr << "mask size = " << g << endl
           << endl;

   adiak::value("num_procs", size);
   adiak::value("array_size", len);
   adiak::value("program_name", "mpi_radix_sort_github");
   adiak::value("array_datatype_size", sizeof(unsigned int));

   CALI_MARK_BEGIN(array_creation);

   vector<unsigned int> *arr = new vector<unsigned int>(len / size);
   test_data(arr, id, len / size);

   CALI_MARK_END(array_creation);

   if (id == 0)
      cerr << "starting radix sort...";
   MPI_Barrier(MPI_COMM_WORLD);

   radix_mpi(arr, id, size, g);

   MPI_Barrier(MPI_COMM_WORLD);
   if (id == 0)
      cerr << "finished" << endl
           << endl;

   MPI_Barrier(MPI_COMM_WORLD);

   CALI_MARK_BEGIN(sort_validation);

   int order = check_array_order(arr, id, size);
   switch (order)
   {
   case ORDER_CORRECT:
      cerr << "TEST: PASSED\n"
           << endl;
      break;
   case ORDER_ONLY_MASTER:
      break;
   default:
      cerr << "TEST: FAILED\n"
           << order << endl;
      break;
   }

   CALI_MARK_END(sort_validation);

   CALI_MARK_END(whole_function);

   char msg[MSG_SIZE];
   sprintf(msg, "Process %d finished.\n", id);
   ordered_print(msg, id, size);

   mgr.stop();
   mgr.flush();

   MPI_Finalize();
}
