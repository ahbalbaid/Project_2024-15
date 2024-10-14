#include <iostream>
#include <stdlib.h>
#include <time.h>
#include <mpi.h>
#include <caliper/cali.h>
#include <caliper/cali-manager.h>
#include <adiak.hpp>
#include <string>

using std::string;
using std::cout;
using std::endl;

const char *whole_function = "whole_function";
const char *array_creation = "array_creation";
const char *master_sending = "master_sending";
const char *merging = "merging";
const char *sort_validation = "sort_validation";

void merge(float *arr, float *temp, int left, int mid, int right)
{
    int leftStart = left, rightStart = mid + 1, current = left;

    while (leftStart <= mid && rightStart <= right)
    {
        if (arr[leftStart] <= arr[rightStart])
        {
            temp[current++] = arr[leftStart++];
        }
        else
        {
            temp[current++] = arr[rightStart++];
        }
    }

    while (leftStart <= mid)
    {
        temp[current++] = arr[leftStart++];
    }

    while (rightStart <= right)
    {
        temp[current++] = arr[rightStart++];
    }

    for (int i = left; i <= right; i++)
    {
        arr[i] = temp[i];
    }
}

void merge_sort(float *arr, float *temp, int left, int right)
{
    if (left < right)
    {
        int mid = (left + right) / 2;
        merge_sort(arr, temp, left, mid);
        merge_sort(arr, temp, mid + 1, right);
        merge(arr, temp, left, mid, right);
    }
}

int check_sort(const float *array, int array_size)
{
    CALI_MARK_BEGIN(sort_validation);

    for (int i = 0; i < array_size - 1; i++)
    {
        if (array[i + 1] < array[i])
        {
            CALI_MARK_END(sort_validation);
            return 0;
        }
    }

    CALI_MARK_END(sort_validation);
    return 1;
}

void random_array(float *array, int array_size)
{
    for (int i = 0; i < array_size; i++)
    {
        array[i] = static_cast<float>(rand());
    }
}

void sorted_array(float *array, int array_size)
{
    for (int i = 0; i < array_size; i++)
    {
        array[i] = static_cast<float>(i);
    }
}

void reverse_array(float *array, int array_size)
{
    for (int i = 0; i < array_size; i++)
    {
        array[i] = static_cast<float>(array_size - i - 1);
    }
}

void perturbed_array(float *array, int array_size){
    for (int i = 0; i < array_size; i++)
    {
        array[i] = static_cast<float>(i);
    }

    for (int i = 0; i < array_size / 100; i++)
    {
        array[rand() % array_size] = static_cast<float>(rand()); 
    }

}

int main(int argc, char **argv)
{
    int input_sort = 0;
    int num_cores = 0;
    int array_size = 0; 

    string sort_method = "";

    int taskid;
    int numtasks;
    int rc;

    if (argc == 4)
    {
        input_sort = atoi(argv[1]);
        num_cores = atoi(argv[2]);
        array_size = atoi(argv[3]);
    }
    else{
        return 0;
    }

    CALI_MARK_BEGIN(whole_function);
    cali::ConfigManager mgr;
    mgr.start();

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &taskid);
    MPI_Comm_size(MPI_COMM_WORLD, &numtasks);
    if (numtasks < 2)
    {
        cout << "program needs more than 1 task." << endl;
        MPI_Abort(MPI_COMM_WORLD, rc);
        exit(1);
    }

    if (taskid == 0)
    {
        cout << "sorting type: " << input_sort << endl;
        cout << "number of cores: " << num_cores << endl;
        cout << "array size: " << array_size << endl;
    }

    float *original_array = new float[array_size];

    srand(time(NULL));

    std::string sorting_type_name;

    CALI_MARK_BEGIN(array_creation);
    if(input_sort == 0){
        sort_method = "random";
        random_array(original_array, array_size);
    }
    else if(input_sort == 1){
        sort_method = "sorted";
        sorted_array(original_array, array_size);
    }
    else if (input_sort == 2){
        sort_method = "reversed";
        reverse_array(original_array, array_size);
    }
    else if (input_sort == 3){
        sort_method = "perturbed";
        perturbed_array(original_array, array_size);
    }

    CALI_MARK_END(array_creation);

    double start_time = MPI_Wtime();

    // get sub array size
    int sub_array_size = array_size / num_cores;

    // send sub arrays to other cores
    CALI_MARK_BEGIN(master_sending);
    float *sub_array = new float[sub_array_size];
    CALI_MARK_BEGIN("mpi_scatter");
    MPI_Scatter(original_array, sub_array_size, MPI_FLOAT, sub_array, sub_array_size, MPI_FLOAT, 0, MPI_COMM_WORLD);
    CALI_MARK_END("mpi_scatter");
    CALI_MARK_END(master_sending);

    // do merge sort on other cores
    CALI_MARK_BEGIN(merging);
    float *tmp_array = new float[sub_array_size];
    merge_sort(sub_array, tmp_array, 0, sub_array_size - 1);

    CALI_MARK_END(merging);

    // gather sub arrays into one 
    CALI_MARK_BEGIN(master_sending);
    float *sorted = nullptr;
    if (taskid == 0)
    {
        sorted = new float[array_size];
    }
    CALI_MARK_BEGIN("mpi_gather");
    MPI_Gather(sub_array, sub_array_size, MPI_FLOAT, sorted, sub_array_size, MPI_FLOAT, 0, MPI_COMM_WORLD);
    CALI_MARK_END("mpi_gather");
    CALI_MARK_END(master_sending);


    if (taskid == 0)
    {

        float *temp_array = new float[array_size];

        // master sorted subarray merge call
        merge_sort(sorted, temp_array, 0, array_size - 1);

        double final_time = MPI_Wtime() - start_time;
        if (check_sort(sorted, array_size) == 0)
        {
            cout << "array was not correctly sorted." << endl;
            return 0;
        }
        else{
            cout << "time for merge sort: " << final_time << " seconds." << endl;
        }

        delete[] sorted;
        delete[] temp_array;
    }

    delete[] original_array;
    delete[] sub_array;
    delete[] tmp_array;

    CALI_MARK_END(whole_function);
    adiak::value("array_size", array_size);                    
    adiak::value("sort_method", sort_method);                          
    adiak::value("num_cores", num_cores);                         

    MPI_Barrier(MPI_COMM_WORLD);

    mgr.stop();
    mgr.flush();
    MPI_Finalize();
}
