#ifndef THREADPOOL_H
#define THREADPOOL_H
#include <iostream>
#include <thread>
#include <functional>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <atomic>
#include <map>
#include <string>
#include <future>


using namespace std;

const int TASK_MAX = 2; // INT32_MAX;
const int THREAD_MAX = 1024;
const int THREAD_IDLE_TIME = 60; // 单位：秒

// 线程池支持的模式
enum class PoolMode {
    MODE_FIXED,  // 固定数量的线程
    MODE_CACHED, // 线程数量可动态增长
};

std::map<std::thread::id, int> threadIdMap;
//using threadId = threadIdMap[this_thread::get_id()];

class Thread {
public:
    using ThreadFunc = std::function<void(int)>;

    Thread(ThreadFunc func) : func_(func), threadId_(generateId_++) {}

    ~Thread() = default;

    void threadRun(int i ) {
        std::thread t(func_, threadId_);
        threadIdMap[t.get_id()] = i;

        t.detach();
    }

    // 获取线程id
    int getId()const
    {
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
    ThreadPool()
        : coreThreadSize_(8)

//      : coreThreadSize_(std::thread::hardware_concurrency())
        , taskSize_(0)
        , idleThreadSize_(0)
        , curThreadSize_(0)
        , taskQueMaxSize_(TASK_MAX)
        , threadMaxSize_(THREAD_MAX)
        , poolMode_(PoolMode::MODE_FIXED)
        , isPoolRunning_(false)
    {}

    ThreadPool(const ThreadPool &) = delete;
    ThreadPool &operator=(const ThreadPool &) = delete;


    ~ ThreadPool() {

        isPoolRunning_ = false;
        // 唤醒等待的线程
        notEmpty_.notify_all();

        std::unique_lock<std::mutex> lock(taskQueMtx_);
        // 等待所有的线程退出: 1.正在运行的, 2. 阻塞着的
        exitCond_.wait(lock, [&]()->bool{return threadsContainer_.size() == 0;});
    }


    void start(int coreThreadSize)
    {
        isPoolRunning_ = true;
        coreThreadSize_ = coreThreadSize;
        curThreadSize_ = coreThreadSize_;

        //创建核心线程数
        for(int i = 0; i < coreThreadSize_; i++)
        {
            //Thread 绑定 consumer
            auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::consumer, this,  std::placeholders::_1));
            int threadId = ptr->getId();
            threadsContainer_.emplace(threadId, std::move(ptr));
        }

        for(int i = 0; i < coreThreadSize_; i++)
        {
            //执行 consumer,开启核心线程让其等待任务的到来
            threadsContainer_[i]->threadRun(i);
            // 执行完任务会返回这里, 记录空闲的线程
            idleThreadSize_++;
        }
    }


    template<typename Func, typename... Args>
    auto producer(Func&& func , Args&&... args) -> std::future<decltype(func(args...))>

    {
        using RType = decltype(func(args...));
        auto task = std::make_shared<std::packaged_task<RType()>>
                (std::bind(std::forward<Func>(func), std::forward<Args>(args)...));
        std::future<RType> res = task->get_future();

        std::unique_lock<std::mutex> lock(taskQueMtx_);
        //任务队列满了
        if (taskQue_.size() > taskQueMaxSize_) {
            cout << "task queue is full" << endl;
            auto task = std::make_shared<std::packaged_task<RType()>>(
                    []()->RType { return RType(); });
            (*task)();
            return task->get_future();

        }
        taskQue_.emplace([task](){
            (*task)();
        });

        std::cout << "---------create core  thread------------" << std::endl;
        notEmpty_.notify_all();

        if(poolMode_ == PoolMode::MODE_CACHED && taskQue_.size() > idleThreadSize_ && curThreadSize_ < threadMaxSize_)
        {
            std::cout << "-------create uncore thread--------"<<std::endl;

            // 创建新的线程对象
            auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::consumer, this, std::placeholders::_1));
            int threadId = ptr->getId();
            threadsContainer_.emplace(threadId, std::move(ptr));
            // 启动线程
            threadsContainer_[threadId]->threadRun(threadId);
            // 修改线程个数相关的变量
            curThreadSize_++;
            idleThreadSize_++;

        }



        return res ;
    }

private:
    void consumer(int threadId)
    {

        auto lastTime = std::chrono::high_resolution_clock().now();
        while (true) {
            std::function<void()> task;
            {

                //先获取锁
                std::unique_lock<std::mutex> lock(taskQueMtx_);
                std::cout  << "thread" << threadIdMap[this_thread::get_id()] << ": "<< "  get mutex..." << std::endl;

                while(taskQue_.size() == 0)
                {
                    if(!isPoolRunning_)
                    {
                        //释放线程
                        threadsContainer_.erase(threadId);
                        std::cout<< "thread"<< threadIdMap[this_thread::get_id()] << ": exit "<<std::endl;
                        exitCond_.notify_all();
                        return;
                    }
                    std::cout<< "thread"<< threadIdMap[this_thread::get_id()] << ": wait "<<std::endl;
                    if (poolMode_ == PoolMode::MODE_CACHED)
                    {
                        if(std::cv_status::timeout == notEmpty_.wait_for(lock, std::chrono::seconds(1)))
                        {
                            auto now = std::chrono::high_resolution_clock().now();
                            auto dur = std::chrono::duration_cast<std::chrono::seconds>(now - lastTime);
                            if(dur.count() >= THREAD_IDLE_TIME && curThreadSize_ > coreThreadSize_)
                            {
                                threadsContainer_.erase(threadId); // std::this_thread::getid()
                                curThreadSize_--;
                                idleThreadSize_--;
                                std::cout<< "thread"<< threadIdMap[this_thread::get_id()] << ": exit "<<std::endl;
                                return;
                            }
                        }

                    }
                    else
                    {
                        notEmpty_.wait(lock);
                    }
                }
                //消费任务
                task = taskQue_.front();
                taskQue_.pop();

                //如果还有任务继续通知
                if(taskQue_.size() > 0)
                {
                    notEmpty_.notify_all();
                }
            }
            if (task)
            {
                std::cout<< "thread"<< threadIdMap[this_thread::get_id()] << ": exec task  "<<std::endl;
                task(); // 执行function<void()>
            }

            idleThreadSize_++;
            lastTime = std::chrono::high_resolution_clock().now(); // 更新线程执行完任务的时间
        }
    }


private:

    // 检查pool的运行状态
    bool checkRunningState() const {
        return isPoolRunning_;
    }

public:
    // 设置线程池的工作模式
    void setMode(PoolMode mode) {
        if (checkRunningState())
            return;
        poolMode_ = mode;
    }


private:
    unordered_map<int, unique_ptr<Thread>> threadsContainer_;
    vector<std::thread> threadContainer_;

    int coreThreadSize_;
    int threadMaxSize_;
    std::atomic_int curThreadSize_;
    std::atomic_int idleThreadSize_;

    queue<function<void()>> taskQue_;
    std::atomic_int taskSize_;
    int taskQueMaxSize_;

    thread consumerThread;

    Thread *factory_;
    int consumeCount_ = 0;

    mutex taskQueMtx_;
    condition_variable notEmpty_;
    std::condition_variable exitCond_; // 等待线程资源全部回收

    PoolMode poolMode_;
    std::atomic_bool isPoolRunning_;

};
#endif

