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
    this_thread::sleep_for(chrono::seconds(15));
    // 比较耗时
    return a + b;
}


int main()
{

    ThreadPool* pool = &ThreadPool::GetInstance();
    pool->startup();

    std::this_thread::sleep_for(std::chrono::seconds(3));  // 暖机

    future<int> r1 = pool->submitTask(sum1, 1, 2);
    future<int> r2 = pool->submitTask(sum1, 1, 2);
    future<int> r3 = pool->submitTask(sum1, 1, 2);
    future<int> r4 = pool->submitTask(sum1, 1, 2);   // 线程阈值
    future<int> r5 = pool->submitTask(sum1, 1, 2);
    future<int> r6 = pool->submitTask(sum1, 1, 2);
    future<int> r7 = pool->submitTask(sum1, 1, 2);
    future<int> r8 = pool->submitTask(sum1, 1, 2);   // 线程阈值 + 任务阈值
    future<int> r9 = pool->submitTask(sum1, 1, 2);
    future<int> r10 = pool->submitTask(sum1, 1, 2);
    future<int> r11 = pool->submitTask(sum1, 1, 2);
    future<int> r12 = pool->submitTask(sum1, 1, 2);
    future<int> r13 = pool->submitTask(sum1, 1, 2);
    future<int> r14 = pool->submitTask(sum1, 1, 2);
    future<int> r15 = pool->submitTask(sum1, 1, 2);
    future<int> r16 = pool->submitTask(sum1, 1, 2);
    // future<int> r17 = pool->submitTask(sum1, 1, 2);
    // future<int> r18 = pool->submitTask(sum1, 1, 2);
    // future<int> r19 = pool->submitTask(sum1, 1, 2);
    // future<int> r20 = pool->submitTask(sum1, 1, 2);
    // future<int> r21 = pool->submitTask(sum1, 1, 2);

    std::cin.get();

    return 0;
}