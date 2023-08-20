#include <iostream>
#include <thread>
#include <functional>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <atomic>

using namespace std;

const int TASK_MAX = 2; // INT32_MAX;
const int THREAD_MAX = 1024;
const int THREAD_IDLE_TIME = 60; // 单位：秒

// 线程池支持的模式
enum class PoolMode {
    MODE_FIXED,  // 固定数量的线程
    MODE_CACHED, // 线程数量可动态增长
};

class Thread {
public:
    using ThreadFunc = std::function<void()>;

    Thread() = default;

    Thread(ThreadFunc func) : func_(func), threadId_(generateId_++) {}

    ~Thread() = default;

    thread createThread(const std::function<void()> &func) {
        return thread(func);
    }

    void start() {
        std::thread t(func_);
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
        : coreThreadSize_(std::thread::hardware_concurrency())
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

    ThreadPool(Thread *factory = new Thread()) : factory_(factory) {}

//    ~ ThreadPool() {
//        isPoolRunning_ = false;
//        notEmpty_.notify_one();
//        for (auto &t: threadContainer_) {
//            t.join();
//        }
//        delete factory_;
//    }
//

    void producer(function<void()> func) {
        std::unique_lock<std::mutex> lock(taskQueMutex);


        if (threadContainer_.size() < coreThreadSize_) {
            threadContainer_.push_back(factory_->createThread(std::bind(&ThreadPool::consumer, this)));
            std::cout << "thread create" << std::endl;
        }


        //任务队列满了
        if (taskQue_.size() > taskQueMaxSize_) {
            cout << "task queue is full" << endl;
            return;
        }
        taskQue_.push(func);
        std::cout << "producer add task" << std::endl;
        notEmpty_.notify_all();
    }

private:
    void consumer() {
        while (true) {
            std::function<void()> task;
            {
                std::unique_lock<std::mutex> lock(taskQueMutex);
                notEmpty_.wait(lock, [this] { return !taskQue_.empty() || isPoolRunning_ == false; });
                if (isPoolRunning_ && taskQue_.empty())
                    break;

                task = taskQue_.front();
                taskQue_.pop();
            }
            if (task) {
                std::cout << "consumer consume task" << endl;
                task();

            }

        }
    }


private:

    // 检查pool的运行状态
    bool checkRunningState() const {
        return isPoolRunning_;
    }

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

    mutex taskQueMutex;
    condition_variable notEmpty_;



    PoolMode poolMode_;
    std::atomic_bool isPoolRunning_;

};

