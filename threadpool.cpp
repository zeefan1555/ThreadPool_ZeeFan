#include "threadpool.h"


void myTask() {
//    std::cout << "------------thread"<<threadIdMap[ std:: this_thread::get_id()] << ": run task ---------------" << std::endl;
    std::cout << std::endl;
}


int main() {
    {
        ThreadPool obj;
        obj.start();

        for (int i = 0; i < 8; i++) {
            obj.producer(myTask);
        }

    }
    std::cout<<"-----------main thread eixt-------------"<<std::endl;

    std::this_thread::sleep_for(std::chrono::seconds(3));

    return 0;
}