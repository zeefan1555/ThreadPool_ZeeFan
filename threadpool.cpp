#include <iostream>
#include <thread>
#include <functional>
#include <queue>
#include <mutex>
#include <condition_variable>

using namespace std;


class ZeefanExecutorV2
{
public:
   ZeefanExecutorV2()
   {
    consumerThread = thread(&ZeefanExecutorV2::consumer, this); // 运行 worker 线程函数
   } 
   ~ ZeefanExecutorV2()
   {
        isPoolRunning_ = false;
        condition.notify_one();
        consumerThread.join();
   }

   void producer(function<void()>func)
   {
    std::unique_lock<std::mutex>lock (queueMutex);
    //任务队列满了
    if(workQueue.size() > maxQueueSize)
    {
        cout<< "task queue is full" <<endl;
        return;
    }
    workQueue.push(func);
    std::cout << "producer add task"<<std::endl;
    condition.notify_one();

   }
    
private:
    void consumer()
    {
       while(true)
       {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            condition.wait(lock, [this]{return !workQueue.empty() || isPoolRunning_ == false;});
            if(isPoolRunning_ && workQueue.empty())
                break;
            
            task = workQueue.front();
            workQueue.pop();
        }
        if(task){
            std::cout<<"consumer consume task" << endl;
            task(); 

        }
            

       } 
    }
    
private:
    queue<function<void()>> workQueue;
    thread consumerThread;
    mutex queueMutex;
    condition_variable condition;
    const size_t maxQueueSize = 3;
    bool isPoolRunning_ = true;

};





void myTask()
{
    std::cout<<"task running" << std::endl;
    std::cout<< std::endl;
}


int main()
{
    ZeefanExecutorV2 obj;

    obj.producer(myTask);
    obj.producer(myTask);
    obj.producer(myTask);
    obj.producer(myTask);
    obj.producer(myTask);
    obj.producer(myTask);
    obj.producer(myTask);
    obj.producer(myTask);


    std::this_thread::sleep_for(std::chrono::seconds(1));

    return 0;
}