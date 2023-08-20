#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <iostream>
#include <vector>
#include <queue>
#include <memory>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <unordered_map>
#include <thread>
#include <future>

using namespace std;

enum class PoolMode {
    MODE_FIXED,  // 固定数量的线程
    MODE_CACHED, // 线程数量可动态增长
};


const int TASK_MAX_THRESHHOLD = 2; // INT32_MAX;

const int THREAD_MAX_THRESHHOLD = 1024;// 非核心线程
const int THREAD_MAX_IDLE_TIME = 60; // 非核心线程的超时时间, 单位：秒



class Thread {
public:
    using ThreadFunc = std::function<void(int)>;

    Thread(ThreadFunc func)
            : func_(func), threadId_(generateId_++) {}

    ~Thread() = default;

    void start() {
        std::thread t(func_, threadId_);
        t.detach();
    }

    int getId() const {
        return threadId_;
    }

private:
    ThreadFunc func_;

    static int generateId_;
    int threadId_;

};

int Thread::generateId_ = 0;


class ThreadPool {
public:
    // 线程池的构造
    ThreadPool()
    	: initThreadSize_(0)
        , taskSize_(0)
        , idleThreadSize_(0)
        , curThreadSize_(0)

        , taskQueMaxThreshHold_(TASK_MAX_THRESHHOLD)
        , threadSizeThreshHold_(THREAD_MAX_THRESHHOLD)

        , poolMode_(PoolMode::MODE_FIXED)
        , isPoolRunning_(false)
    {}

    ~ThreadPool()
    {
        isPoolRunning_ = false;
        std::unique_lock<std::mutex> lock(taskQueMtx_);
        notEmpty_.notify_all();
        exitCond_.wait(lock,[&]()->bool{return threads_.size() == 0;});

    }
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    bool checkRunningState() const
    {
        return isPoolRunning_;
    }

    void setMode(PoolMode mode)
    {
        if(checkRunningState())
            return; // 线程池启动不允许设置模式, 只允许在启动前设置
        poolMode_ = mode;
    }

    void setTaskQueThreshHold(int threshhold) // 最大任务数
    {
        if(checkRunningState())
            return;
        taskQueMaxThreshHold_ = threshhold;
    }

    void setTreadSizeThreshHold(int threshhold) // 最大线程数
    {
        if(checkRunningState())
            return;
        if(poolMode_ == PoolMode::MODE_CACHED)
        {
            threadSizeThreshHold_ = threshhold; //只允许cached 模式下能够创建非核心线程
        }
    }

template<typename Func, typename...Args>
auto submitTask(Func&& func, Args&&... args)->std::future<decltype(func(args...))>
{
        //打包任务放到任务队列当中
        using RType = decltype(func(args...));
        auto task = std::make_shared<std::packaged_task<RType>>(
                std::bind(std::forward<Func>(func), std::forward<Args>(args)...));
        std::future<RType> result = task->get_future();


        //获取锁
        std::unique_lock<std::mutex> lock(taskQueMtx_);

        //任务提交失败
        if(!notFull_.wait_for(lock, std::chrono::seconds(1),
          [&]()->bool {return taskQue_.size() < (size_t)taskQueMaxThreshHold_;}))
        {
			std::cerr<<"task queue is full, submit task fail" << std::endl;
            auto task = std::make_shared<std::packaged_task<RType()>>(
                    []()->RType{return RType();});
            (*task)();
            return task->get_future();
        }

        //队列有空闲, 放任务到队列当中
        taskQue_.emplace([task]() {(*task)();});
        taskSize_++;

    	//放了新任务, 所以任务队列不空, 通知线程来执行
        notEmpty_.notify_all();

        //cached 模式, 动态增加非核心线程
        if(poolMode_ == PoolMode::MODE_CACHED
        	&& taskSize_ > initThreadSize_             // 任务超过了核心线程,
            && curThreadSize_ < threadSizeThreshHold_) // 且当前线程小于线程的阈值
        {
            std::cout << "start create dynamic thread....." << std::endl;

            auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this , std::placeholders::_1));
            int threadId = ptr->getId();
            threads_.emplace(threadId, std::move(ptr));

            threads_[threadId]->start();

            curThreadSize_++;
            idleThreadSize_++;
        }
        return result;

}


void start(int initThreadSize = std::thread::hardware_concurrency())
{
    isPoolRunning_ = true;

    initThreadSize_ = initThreadSize;
    curThreadSize_ = initThreadSize;

    for(int i = 0; i < initThreadSize_; ++i){
        // 创建thread线程对象的时候，把线程函数给到thread线程对象
        auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));
        int threadId  = ptr->getId();
        threads_.emplace(threadId, std::move(ptr));
    }
    for(int i = 0; i < initThreadSize_; i++)
    {
        threads_[i]->start();
        idleThreadSize_++; // 记录初始空闲线程的数量
    }
}

private:
    //线程函数
    void threadFunc(int threadid)
    {
        auto lastTime = std::chrono::high_resolution_clock().now();

        // 所有任务必须执行完成，线程池才可以回收所有线程资源
        for(;;)
        {
            Task task;
            {
                std::unique_lock<std::mutex> lock(taskQueMtx_);
                std::cout<<"tid:" << std::this_thread::get_id() << ": try get task..." << std::endl;

                // cached模式下创建了非核心线程，但是空闲时间超过60s，回收掉
                // 当前时间 - 上一次线程执行的时间 > 60s


                // 每一秒中返回一次   怎么区分：超时返回？还是有任务待执行返回
                // 锁 + 双重判断
                while(taskQue_.size() == 0)
                {
                    // 线程池要结束，回收线程资源
                    if(!isPoolRunning_)
                    {
                        threads_.erase(threadid);
                        std::cout<<"threadid: " << std::this_thread::get_id() << "exit!"<< std::endl;
                        exitCond_.notify_all();
                        return; // 线程函数结束, 线程结束
                    }

                    if(poolMode_ == PoolMode::MODE_CACHED)
                    {
                        if(std::cv_status::timeout == notEmpty_.wait_for(lock, std::chrono::seconds(1)))
                        {
                            auto now = std::chrono::high_resolution_clock().now();
                            auto dur = std::chrono::duration_cast<std::chrono::seconds>(now - lastTime);
                            //回收非核心线程
                            if(dur.count() >= THREAD_MAX_IDLE_TIME && curThreadSize_ > initThreadSize_)
                            {
                                threads_.erase(threadid);
                                curThreadSize_--;
                                idleThreadSize_--;

                                std::cout<<"threadid"<< std::this_thread::get_id() << "exit!" << std::endl;
                                return ;
                            }
                        }
                    }
                    else
                    {
                        notEmpty_.wait(lock);
                    }
                }
                //获取任务成功
                idleThreadSize_--;
                std::cout<< "tid : " << std::this_thread::get_id() << ": get task success.." << std::endl;

                task = taskQue_.front();
                taskQue_.pop();
                taskSize_--;

                // 如果依然有剩余任务，继续通知其它得线程执行任务
                if(taskQue_.size() > 0)
                {
                   notEmpty_.notify_all();
                }

                // 取出一个任务，进行通知，通知可以继续提交生产任务
                notFull_.notify_all();
            }

            if(task != nullptr)
            {
                task();//
            }

            //线程执行任务执行完了
            idleThreadSize_++;
            lastTime = std::chrono::high_resolution_clock().now(); // 更新线程执行完任务的时间
        }

    }

private:
    unordered_map<int, unique_ptr<Thread>> threads_;

    //核心线程数量
    int initThreadSize_;
    //最大线程数
    int threadSizeThreshHold_;

    std::atomic_int curThreadSize_;

    //空闲线程数量
    std::atomic_int idleThreadSize_;

    // task -> function
    using Task = std::function<void()>; //执行函数
    queue <Task> taskQue_;
    atomic_int taskSize_;
    int taskQueMaxThreshHold_; // 任务队列上线

    mutex taskQueMtx_;
    condition_variable notFull_;
    condition_variable notEmpty_;
    condition_variable exitCond_;

    PoolMode poolMode_;
    atomic_bool isPoolRunning_;


};


#endif //THREADPOOL_H