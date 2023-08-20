// 线程池项目-最终版.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include <iostream>
#include <functional>
#include <thread>
#include <future>
#include <chrono>
using namespace std;

#include "threadpool.h"




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
   thread t1{sum1, 1,2};
   thread t2{sum2, 1,2,3};
   t1.join();
   t2.join();

}