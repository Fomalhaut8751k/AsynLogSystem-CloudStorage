#include <iostream>
#include <functional>
#include <thread>
#include <future>
#include <chrono>
#include <ctime>
using namespace std;

#include "ThreadPool.hpp"

/*
    运行此代码可以比较使用线程池与否对计算速度的影响
*/

int sum1(int a, int b)
{
    this_thread::sleep_for(chrono::seconds(2));
    // 比较耗时
    return a + b;
}
int sum2(int a, int b, int c)
{
    this_thread::sleep_for(chrono::seconds(2));
    return a + b + c;
}

int main()
{

    ThreadPool* pool = &ThreadPool::GetInstance();
    pool->startup();

    auto begin1 = std::chrono::high_resolution_clock::now();
    long long result1 = 0;
    for(long long i = 1; i <= 3000000000; i++)
    {
        result1 += i;
    }
    auto end1 = std::chrono::high_resolution_clock::now();

    cout << "不使用线程池: result = " << result1<<  ", 用时: " << (end1 - begin1).count() << "ms" << endl;


    auto begin2 = std::chrono::high_resolution_clock::now();
    future<long long> r1 = pool->submitTask([](long long b, long long e)->long long {
        long long sum = 0;
        for (long long i = b; i <= e; i++)
            sum += i;
        return sum;
        }, 1, 1000000000);
    future<long long> r2 = pool->submitTask([](long long b, long long e)->long long {
        long long sum = 0;
        for (long long i = b; i <= e; i++)
            sum += i;
        return sum;
        }, 1000000001, 2000000000);
    future<long long> r3 = pool->submitTask([](long long b, long long e)->long long {
        long long sum = 0;
        for (long long i = b; i <= e; i++)
            sum += i;
        return sum;
        }, 2000000001, 3000000000);


    long long result2 =  r1.get() + r2.get() + r3.get();
    auto end2 = std::chrono::high_resolution_clock::now();
    
    cout << "使用线程池: result = " << result2 <<  ", 用时: " << (end2 - begin2).count() << "ms" << endl;


    return 0;
}