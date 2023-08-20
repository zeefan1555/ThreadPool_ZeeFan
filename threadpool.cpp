#include <iostream>
#include <thread>
#include <functional>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <vector>

using namespace std;


class ThreadFactory
{
public:

    thread createThread(const std::function<void()>& func)
    {
        return thread(func);
    }
private:

    static int generateId_;
    int threadId_;
};


class Thread
{
public:
   Thread(ThreadFactory* factory = new ThreadFactory()) : factory_(factory){}

   ~ Thread()
   {
        isPoolRunning_ = false;
        condition.notify_one();
        for(auto& t : threadContainer_)
        {
            t.join();
        }
        delete factory_;

   }

   void producer(function<void()>func)
   {
        std::unique_lock<std::mutex>lock (queueMutex);

        //创建核心线程
        for(int i = 0;  i < coreThreadSize_; i++)
        {
            threadContainer_.push_back(factory_->createThread(std::bind(&Thread::consumer, this)));
        }

        //任务队列满了
        if(taskQueue.size() > maxQueueSize)
        {
            cout<< "task queue is full" <<endl;
            return;
        }
        taskQueue.push(func);
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
            condition.wait(lock, [this]{return !taskQueue.empty() || isPoolRunning_ == false;});
            if(isPoolRunning_ && taskQueue.empty())
                break;
            
            task = taskQueue.front();
            taskQueue.pop();
        }
        if(task){
            std::cout<<"consumer consume task" << endl;
            task(); 

        }
            

       } 
    }
    
private:
    queue<function<void()>> taskQueue;
    thread consumerThread;
    mutex queueMutex;
    condition_variable condition;
    const size_t maxQueueSize = 3;
    bool isPoolRunning_ = true;
    int coreThreadSize_ = std::thread::hardware_concurrency();
    vector<std::thread> threadContainer_;
    ThreadFactory* factory_;
};





void myTask()
{
    std::cout<<"task running" << std::endl;
    std::cout<< std::endl;
}


int main()
{
    Thread obj;

    for(int i = 0; i < 20; i++)
    {
        obj.producer(myTask);
    }



    std::this_thread::sleep_for(std::chrono::seconds(1));

    return 0;
}