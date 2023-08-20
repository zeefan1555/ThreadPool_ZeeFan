#include "threadpool.h"


void myTask() {
    std::cout << "task running" << std::endl;
    std::cout << std::endl;
}


int main() {
    ThreadPool obj;


    for (int i = 0; i < 20; i++) {
        obj.producer(myTask);
    }


    std::this_thread::sleep_for(std::chrono::seconds(3));

    return 0;
}