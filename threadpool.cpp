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
    workerThread = thread(&ZeefanExecutorV2::consumer, this); // 运行 worker 线程函数
   } 
   ~ ZeefanExecutorV2()
   {
        terminate = true;
        condition.notify_one();
        workerThread.join();
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
            condition.wait(lock, [this]{return !workQueue.empty() || terminate == true;});
            if(terminate && workQueue.empty())
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
    thread workerThread;
    mutex queueMutex;
    condition_variable condition;
    const size_t maxQueueSize = 3;
    bool terminate = false;

    

};





void myFuntion()
{
    std::cout<<"thread running from function " << std::endl;
    std::cout<< std::endl;
}


int main()
{
    ZeefanExecutorV2 obj;

    obj.producer(myFuntion);
    obj.producer(myFuntion);
    obj.producer(myFuntion);
    obj.producer(myFuntion);
    obj.producer(myFuntion);
    obj.producer(myFuntion);
    obj.producer(myFuntion);
    obj.producer(myFuntion);


    std::this_thread::sleep_for(std::chrono::seconds(1));

    return 0;
}