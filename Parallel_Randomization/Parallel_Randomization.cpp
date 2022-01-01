#include <iostream>
#include <future>
#include <omp.h>
#include <thread>
#include <vector>
#include <Windows.h>

#define randomize ((mulA * seedI) + offsetB) % mod

unsigned mod = (unsigned)std::pow(2, 32) - 1;

unsigned mulA = rand() % mod;
unsigned offsetB = rand() % mod;

unsigned numOfThreads = std::thread::hardware_concurrency();


struct vectorType {
    unsigned value;
    vectorType* next;
};

using std::vector;
using std::thread;


//-----------------------Однопоточный метод---------------------------

vectorType* vecRandLinear(unsigned length, unsigned seedI) {
    vectorType* elem = new vectorType();

    if (1 < length) {
        elem->next = vecRandLinear(length - 1, randomize);
    }

    if (1 > length)
        return NULL;

    elem->value = seedI;

    return elem;
}


//-----------------------------Многопоточный метод через CS-----------------------------


/*По сути что происходит:
 * создаём количество голов списков, равное количеству потоков
 * запоминается ссылка на голову
 * в каждой из этих голов происходят следующие действия:
 * -> добавляется следующий элемент в конец списка
 * -> генерируется новое значение элемента списка на основе зерна и формулы рандомизации
 * -> процесс повторяется, пока длина каждого такого списка не равна length / numOfThreads
 * после этого концы списков склеиваются со следующими после них головами
 * оставшиеся элементы, которые были отрезаны как остаток от деления length / numOfThreads,
 * вычисляются доконца линейно
*/
vectorType* vecRandParallel(unsigned length, unsigned seedI) {
    vectorType* firstElem = new vectorType();

    //Инициализация критической секции
    CRITICAL_SECTION cs;
    InitializeCriticalSection(&cs);

    //Получаем количество потоков
    unsigned T = numOfThreads;

    //Инициализация массива значений потоков
    vector<vectorType*> results(T);
    for (unsigned i = 0; i < T; i++)
        results[i] = new vectorType();

    //Сохранение первых элементов
    vector<vectorType*> firstElementsOfRes(T);
    for (unsigned i = 0; i < T; i++)
        firstElementsOfRes[i] = results[i];

    unsigned numOfThreadSteps = length / T;

    //Следующий элемент -> перевычисление зерна -> применение значения -> повторить
    auto thread_proc = [=, &results, &cs, &seedI](unsigned t) {

        for (unsigned i = 0; i < numOfThreadSteps; i++) {
            vectorType* nextElem = new vectorType();

            results[t]->next = nextElem;

            results[t] = results[t]->next;

            EnterCriticalSection(&cs);
            seedI = randomize;
            LeaveCriticalSection(&cs);

            results[t]->value = seedI;
        }

    };

    //Создаем массив исполняющих потоков
    vector<thread> threads;

    for (unsigned t = 1; t < T; t++)
        threads.emplace_back(thread_proc, t);

    thread_proc(0);

    //Инициализируем выполнение каждого из потоков
    for (auto& thread : threads)
        thread.join();

    //Записываем результаты вычислений каждого из потоков в один общий список
    for (unsigned i = 0; i < T - 1; i++) {
        results[i]->next = firstElementsOfRes[i + 1]->next;
    }
    firstElem = firstElementsOfRes[0]->next;

    //Дополняем вычисленные многопоточно элементы однопоточными, которые не вошли в степень количества операций от потоков
    if (firstElem != NULL)
        results[T - 1]->next = vecRandLinear(length - (numOfThreadSteps * T), randomize);
    else firstElem = vecRandLinear(length - (numOfThreadSteps * T), randomize);

    //Очистка памяти
    for (unsigned i = 0; i < T; i++)
        firstElementsOfRes[i] = NULL;
    DeleteCriticalSection(&cs);

    return firstElem;
}


//-----------------------------Многопоточный async метод-----------------------------


unsigned vecRandAsyncProc(unsigned length, unsigned seedI, vectorType* prevElem) {

    if (1 < length) {
        vectorType* elem = new vectorType();
        prevElem->next = elem;
        elem->value = std::async(std::launch::async, vecRandAsyncProc, length - 1, randomize, elem).get();
    }

    return seedI;
}


vectorType* vecRandAsyncParallel(unsigned length, unsigned seedI) {
    vectorType* elem = new vectorType();

    elem->value = vecRandAsyncProc(length, seedI, elem);

    return elem;
}


//--------------------------------------------------------------------


int main()
{
    unsigned randType = 2;

    unsigned length = 2048;

    unsigned seed = 228;
    unsigned seedI = seed;

    vectorType* vec = new vectorType();

    double t0 = omp_get_wtime();

    if (randType == 0)
        for (unsigned i = 0; i < length; i++)
            vec = vecRandLinear(length, randomize);

    if (randType == 1)
        for (unsigned i = 0; i < length; i++)
            vec = vecRandAsyncParallel(length, randomize);

    if (randType == 2)
        for (unsigned i = 0; i < length; i++)
            vec = vecRandParallel(length, randomize);

    vectorType* l_vec = vec;

    int i = 1;

    std::cout << "Resulting time is: " << omp_get_wtime() - t0 << std::endl << std::endl;

    /*
    while (l_vec != NULL) {
        std::cout << i++ << " " << l_vec->value << std::endl;
        l_vec = l_vec->next;
    }
    */
}