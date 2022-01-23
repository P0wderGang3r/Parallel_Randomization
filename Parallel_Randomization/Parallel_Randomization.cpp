#include <iostream>
#include <future>
#include <omp.h>
#include <thread>
#include <vector>

using std::vector;
using std::thread;

#define randSeedI ((mulA * seedI) + offsetB) % mod

#define valBounds (max - min + 1) + min

unsigned numOfThreads = std::thread::hardware_concurrency();

struct vectorType {
    unsigned value;
};

unsigned mod = 4294967295; //(2^32 - 1)
unsigned mulA = 134775813;
unsigned offsetB = 1;

#define isAccumulatorNeeded true

#if isAccumulatorNeeded
#define accInit vector<double> accumulator(numOfThreads)
#define accResInit double acc = 0
#define accThreadProc , &accumulator
#define accInc accumulator[t] += vec->at(i).value
#define accSum for (auto &accVecVal : accumulator) acc += (accVecVal / vec->size())
#define accOutput std::cout << "Average is " << acc << std::endl
#else
#define accInit
#define accResInit
#define accThreadProc
#define accInc
#define accSum
#define accOutput
#endif


//-----------------------Однопоточный метод---------------------------


void vecRandLinear(unsigned seedI, vector<vectorType>* vec, unsigned min, unsigned max) {
    for (unsigned i = 0; i < vec->size(); i++) {
        seedI = randSeedI;
        vec->at(i).value = seedI % valBounds;
    }
}


//-----------------------Многопоточный метод--------------------------


/*По сути что происходит:
 * Заполняем массив, разбивая его на numOfThreads блоков,
 * с каждым из блоков которого взаимодействует ровно 1 поток.
 * После заполнения таких блоков не вошедшие в них элементы
 * заполняются одним потоком.
*/
#define localOffsetB offsetB * (vecMulA[t] - 1) / (vecMulA[0] - 1)
#define parSeedI vecMulA[t + 1] * seedI + localOffsetB

void vecRandParallel(unsigned seedGlobal, vector<vectorType>* vec, unsigned min, unsigned max) {

    //---> Создаем массив множителей A
    vector<int> vecMulA(numOfThreads + 1);
    vecMulA[0] = mulA;
    for (int i = 1; i < vecMulA.size(); i++) {
        vecMulA[i] = (vecMulA[i - 1] * mulA) % mod;
    }
    //<---
    
    //---> Создаем массив множителей B
    vector<int> vecOffsetB(numOfThreads);
    vecOffsetB[0] = offsetB;
    for (int i = 1; i < vecOffsetB.size(); i++) {
        vecOffsetB[i] = (vecOffsetB[i - 1] + mulA);
    }
    //<---

    //---> Создаем массив сумм всех элементов массива vec
    accInit;
    accResInit;
    //<---
    
    //---> Процесс каждого из ядер
    auto thread_proc = [=, &vec, &vecMulA, &vecOffsetB accThreadProc](unsigned t) {

        unsigned seedI = mulA * seedGlobal + vecOffsetB[t];

        for (unsigned i = t; i < vec->size(); i += numOfThreads) {
            vec->at(i).value = seedI % valBounds;
            accInc;
            for (unsigned j = 0; j < numOfThreads; j++) {
                vecMulA[t] *= mulA % mod;
                vecOffsetB[t] += vecMulA[t];
            }
            vecOffsetB[t] %= mod;
            seedI = mulA * seedI + vecOffsetB[t];
        }
    };
    //<---
    

    //---> Создаем массив исполняющих потоков и инициализируем исполнение каждого из потоков
    vector<thread> threads;

    for (unsigned t = 1; t < numOfThreads; t++)
        threads.emplace_back(thread_proc, t);

    thread_proc(0);

    for (auto& thread : threads)
        thread.join();
    //<---

    accSum;
    accOutput;
}


//----------------------------Сортировка------------------------------

unsigned middle(vector<vectorType>* vec, unsigned left, unsigned right) {
    unsigned pivot = vec->at((left + right) / 2).value;
    unsigned i = left;
    unsigned j = right;
    unsigned buf;
    while (true) {

        while (vec->at(i).value < pivot)
            i = i + 1;
        while (vec->at(j).value > pivot)
            j = j - 1;
        if (i >= j)
            return j;

        buf = vec->at(i).value;
        vec->at(i).value = vec->at(j).value;
        vec->at(j).value = buf;
        i++;
        j--;
    }
}

void vecSort(vector<vectorType>* vec, unsigned left, unsigned right) {
    if (left < right) {
        unsigned m = middle(vec, left, right);

#pragma omp task
        {
            vecSort(vec, left, m);
        }

#pragma omp task
        {
            vecSort(vec, m + 1, right);
        }
    }
}


//--------------------------Очистка вектора---------------------------


void vecZeroing(vector<vectorType>* vec) {
    for (unsigned i = 0; i < vec->size(); i++) {
        vec->at(i).value = 0;
    }
}


//-------------------------------Тесты--------------------------------


void classicTest(unsigned length, unsigned seed, unsigned min, unsigned max, bool isParallel) {

    vector<vectorType> vec(length);

    double t0 = omp_get_wtime();

    if (isParallel == false)
        vecRandLinear(seed, &vec, min, max);

    if (isParallel == true)
        vecRandParallel(seed, &vec, min, max);

    std::cout << "Resulting time is: " << omp_get_wtime() - t0 << std::endl << std::endl;

    for (unsigned i = 0; i < length; i++) {
        std::cout << i << " " << vec[i].value << std::endl;
    }

    vecZeroing(&vec);
}


void speedTest(unsigned length, unsigned seed, unsigned min, unsigned max) {
    vector<vectorType> vec(length);

    for (unsigned i = 1; i <= std::thread::hardware_concurrency(); i++) {

        numOfThreads = i;

        double t0 = omp_get_wtime();

        vecRandParallel(seed, &vec, min, max);
        //std::cout << "Resulting time is " << omp_get_wtime() - t0 << " seconds for " << numOfThreads << " threads" << std::endl << std::endl;

        std::cout << omp_get_wtime() - t0 << std::endl;

        vecZeroing(&vec);
    }

}


void sortTest(unsigned length, unsigned seed, unsigned min, unsigned max) {

    vector<vectorType> vec(length);


    for (unsigned i = 1; i <= std::thread::hardware_concurrency(); i++) {

        //--->Заполнение массива
        numOfThreads = std::thread::hardware_concurrency();
        vecRandParallel(seed, &vec, min, max);
        //<---

        //--->Сортировка
        numOfThreads = i;
        omp_set_num_threads(numOfThreads);

        double t0 = omp_get_wtime();
        
        vecSort(&vec, 0, vec.size() - 1);
        //std::cout << "Resulting time is " << omp_get_wtime() - t0 << " seconds for " << numOfThreads << " threads" << std::endl;
        std::cout << omp_get_wtime() - t0 << std::endl;
        //<---

        /*
        for (unsigned k = 0; k < vec.size(); k++) {
            std::cout << vec.at(k).value << " ";
        }
        std::cout << std::endl << std::endl;
        */

        vecZeroing(&vec);
    }

    omp_set_num_threads(numOfThreads);
}

//--------------------------------------------------------------------


int main()
{
    //freopen("output_parallel.txt", "w", stdout);
    unsigned length = 100;
    unsigned seed = 228;

    unsigned min = 0;
    unsigned max = 1000;

    bool isParallel = true;

    //classicTest(length, seed, min, max, isParallel);
    
    speedTest(length, seed, min, max);

    std::cout << std::endl;

    //sortTest(length, seed, min, max);
}