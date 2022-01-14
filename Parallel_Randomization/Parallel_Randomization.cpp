#include <iostream>
#include <future>
#include <omp.h>
#include <thread>
#include <vector>
#include <Windows.h>

using std::vector;
using std::thread;

#define randSeedI ((mulA * seedI) + offsetB) % mod

#define valBounds (max - min + 1) + min

unsigned numOfThreads = std::thread::hardware_concurrency();

struct vectorType {
    unsigned value;
};

unsigned mod = (unsigned)std::pow(2, 32) - 1;
unsigned mulA = 134775813;
unsigned offsetB = 1;

#define isAccumulatorNeeded false

#if isAccumulatorNeeded
#define accInit vector<unsigned> accumulator(numOfThreads + 1)
#define accResInit unsigned acc = 0
#define accThreadProc , &accumulator
#define accInc accumulator[t] += vec->at(i).value
#define accSum for (auto &accVecVal : accumulator) acc += accVecVal
#define accOutput std::cout << "Average is " << acc / vec->size() << std::endl
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

    //---> Создаем массив сумм всех элементов массива vec
    accInit;
    accResInit;
    //<---

    //---> Процесс каждого из ядер
    auto thread_proc = [=, &vec accThreadProc](unsigned t) {

        unsigned seedI = vecMulA[t] * seedGlobal + localOffsetB;

        for (unsigned i = t; i < vec->size(); i += numOfThreads) {
            vec->at(i).value = seedI % valBounds;
            accInc;
            seedI = parSeedI;
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

    for (unsigned k = 0; k < vec->size(); k++) {
        std::cout << vec->at(k).value << " ";
    }
    std::cout << std::endl;

    if (left < right) {
        unsigned p = middle(vec, left, right);
        vecSort(vec, left, p);
        vecSort(vec, p + 1, right);
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

    vecSort(&vec, 0, vec.size() - 1);

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
        std::cout << "Resulting time is " << omp_get_wtime() - t0 << " seconds for " << numOfThreads << " threads" << std::endl << std::endl;

        //std::cout << omp_get_wtime() - t0 << std::endl;

        /*
        for (unsigned j = 0; j < vec.size(); j++) {
            std::cout << j << " " << vec[j].value << std::endl;
        }
        */

        vecZeroing(&vec);
    }

}


//--------------------------------------------------------------------


int main()
{
    //freopen("output_parallel.txt", "w", stdout);
    unsigned length = 10;
    unsigned seed = 228;

    unsigned min = 0;
    unsigned max = 1000;

    bool testType = false;
    bool isParallel = true;

    if (testType == false)
        classicTest(length, seed, min, max, isParallel);
    else
        speedTest(length, seed, min, max);
}