#include <iostream>
#include <cmath>
#include <cstring>
// https://www.geeksforgeeks.org/measure-execution-time-function-cpp/
#include <chrono>
#include <mpi.h>
using namespace std::chrono;
#include <bits/stdc++.h>

class Natural
{
private:
    int value;
    bool marked;

public:
    Natural(int v = 0)
    {
        value = v;
        marked = false;
    }
    int getValue()
    {
        return value;
    }
    int getSquared()
    {
        return value * value;
    }
    void mark()
    {
        marked = true;
    }
    bool isMarked()
    {
        return marked;
    }
};

template <class T>
class Array
{
private:
    size_t len;

public:
    Array()
    {
        array = nullptr;
        len = 0;
    }

    Array(T *a, size_t s)
    {
        array = a;
        len = s;
    }
    T *array;
    T *getArray()
    {
        return array;
    }
    size_t size()
    {
        return len;
    }

    void concat(T *other, int size)
    {
        size_t new_size = size + this->size();
        T *new_array = new T[new_size + this->size()];
        for (int i = 0; i < this->size(); i++)
        {
            new_array[i] = this->array[i];
        }
        for (int i = 0; i < size; i++)
        {
            new_array[i + this->size()] = other[i];
        }
        this->len = new_size;
        this->array = new_array;
    }

    T &operator[](const int index);
};

template <class T>
T &Array<T>::operator[](const int index)
{
    return array[index];
}

Array<Natural> create_array(size_t min, size_t last_number)
{
    size_t size = min == 0 ? last_number + 1 : last_number - min + 1;
    Natural *array = new Natural[size];
    for (size_t i = 0; i < size; i++)
    {
        array[i] = Natural(i + min);
    }

    if (min == 0)
    {
        array[0].mark();
        array[1].mark();
    }

    return Array<Natural>(array, size);
}

Array<Natural> create_array(size_t max)
{
    return create_array(0, max);
}

int get_result_cnt(Array<Natural> from)
{
    int res_siz = 0;
    for (int i = 0; i < from.size(); i++)
    {
        if (!from[i].isMarked())
        {
            res_siz++;
        }
    }
    return res_siz;
}
int *get_results(Array<Natural> from)
{
    int res_siz = get_result_cnt(from);
    int *results = new int[res_siz];
    res_siz = 0;
    for (int i = 0; i < from.size(); i++)
    {
        if (!from[i].isMarked())
        {
            results[res_siz] = from[i].getValue();
            res_siz++;
        }
    }
    return results;
}

Natural getNextUnmarked(Natural n, Array<Natural> array)
{
    for (size_t i = n.getValue() + 1; i < array.size(); i++)
    {
        if (!array[i].isMarked())
        {
            return array[i];
        }
    }
    return n;
}

Array<int> get_seeds(int max)
{
    int max_squared = (int)floor(sqrt(max));
    int size = max_squared + 1;

    Array<Natural> numbers_until_squared = create_array(max_squared);
    Natural smallest_unmarked = Natural(2);
    int num_of_seeds = 0;
    while (smallest_unmarked.getSquared() <= max)
    {
        num_of_seeds++;
        for (int i = smallest_unmarked.getSquared(); i < size; i += smallest_unmarked.getValue())
        {
            numbers_until_squared[i].mark();
        }
        Natural next_smallest = getNextUnmarked(smallest_unmarked, numbers_until_squared);
        if (next_smallest.getValue() != smallest_unmarked.getValue())
            smallest_unmarked = next_smallest;
        else
            break;
    }

    int *calculated_seeds = (int *)malloc(num_of_seeds * sizeof(int));
    int cnt = 0;
    for (int i = 0; i < size; i++)
    {
        if (!numbers_until_squared[i].isMarked())
            calculated_seeds[cnt++] = numbers_until_squared[i].getValue();
    }
    return Array<int>(calculated_seeds, num_of_seeds);
}

int main(int argc, char **argv)
{
    int MAX_MSG_SIZE = 20000000;
    int rank, num_of_threads;
    int max = std::stoi(argv[1]);
    int min = (int)floor(sqrt(max)) + 1;
    MPI_Status status;

    auto start = high_resolution_clock::now();
    Array<int> seeds;
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &num_of_threads);

    if (rank == 0)
    {
        seeds = get_seeds(max);

        for (int i = 1; i < num_of_threads; i++)
        {
            int *shadow = seeds.getArray();
            MPI_Send(shadow, (int)seeds.size(), MPI_INT, i, 0, MPI_COMM_WORLD);
        }
    }
    else
    {
        int *myseeds = (int *)malloc(MAX_MSG_SIZE * sizeof(int));
        int seeds_size;
        MPI_Recv(myseeds, MAX_MSG_SIZE, MPI_INT, 0, 0, MPI_COMM_WORLD, &status);
        MPI_Get_count(&status, MPI_INT, &seeds_size);
        seeds = Array<int>(myseeds, seeds_size);
    }

    int fragment_size = (max - min) / num_of_threads;
    int top;
    if (rank == num_of_threads - 1)
    {
        top = max;
    }
    else
    {
        top = min + (rank + 1) * fragment_size - 1;
    }
    int bottom = min + (rank * fragment_size); 
    Array<Natural> myFragment;
    if (min + (rank * fragment_size) > top)
    {
        myFragment = create_array(0, 2);
    }
    else
    {
        myFragment = create_array(bottom, top);
    }

    for (int i = 0; i < myFragment.size(); i++)
    {

        for (int seed_id = 0; seed_id < seeds.size(); seed_id++)
        {
            if (myFragment[i].getValue() % seeds[seed_id] == 0)
            {
                myFragment[i].mark();
                break;
            }
        }
    }


    MPI_Send(get_results(myFragment), get_result_cnt(myFragment), MPI_INT, 0, 1, MPI_COMM_WORLD);
    
    if (rank == 0)
    {
        Array<int> results = Array<int>();
        results.concat(seeds.array, seeds.size());
        //results.concat(get_results(myFragment), myFragment.size());
        for (int i = 0; i < num_of_threads; i++)
        {
            int *recieved_values = (int *)malloc(MAX_MSG_SIZE * sizeof(int));
            int recieved_size;
            MPI_Recv(recieved_values, MAX_MSG_SIZE, MPI_INT, i, 1, MPI_COMM_WORLD, &status);
            MPI_Get_count(&status, MPI_INT, &recieved_size);
            results.concat(recieved_values, recieved_size);
        }
        int n = sizeof(results.array) / sizeof(results.array[0]);
        std::sort(results.array, results.array + n);

        auto stop = high_resolution_clock::now();

        if (argc == 4 && strcmp(argv[2], "--time") == 0)
        {
            std::cout << duration_cast<microseconds>(stop - start).count(); //µs
        }
        else
        {
            for (int i = 0; i < results.size(); i++)
            {
                std::cout << results[i] << " ";
            }
            std::cout << std::endl;
        }
    }
    MPI_Finalize();
    return 0;
}