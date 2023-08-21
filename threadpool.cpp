#include "threadpool.h"






void myTask() {
    std::cout << "------------thread"<<threadIdMap[ std:: this_thread::get_id()] << ": run task ---------------" << std::endl;
    std::cout << std::endl;
}

int sum1 (int a, int b)
{
    return a+b;
}

int main() {
    {
        ThreadPool pool;
        pool.start();

        for (int i = 0; i < 33; i++) {
           auto res =  pool.producer(myTask);
        }
        auto  res1 = pool.producer(sum1, 1, 2);
        std::cout << res1.get() << std::endl;

    }
    std::cout<<"-----------main thread eixt-------------"<<std::endl;

    getchar();
//    std::this_thread::sleep_for(std::chrono::seconds(3));

    return 0;
}