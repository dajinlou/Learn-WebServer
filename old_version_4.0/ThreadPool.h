#ifndef THREADPOOL_H_
#define THREADPOOL_H_

#include <pthread.h>
#include <functional>
#include <memory>
#include <vector>

// 错误类型
const int THREADPOOL_INVALID = -1;        // 无效的
const int THREADPOOL_LOCK_FAILURE = -2;   // 加锁失败
const int THREADPOOL_QUEUE_FULL = -3;     // 队列满
const int THREADPOOL_SHUTDOWN = -4;       // 线程池关闭
const int THREADPOOL_THREAD_FAILURE = -5; // 线程失败
const int THREADPOOL_GRACEFUL = 1;        // 是否优雅

const int MAX_THREADS = 1024;
const int MAX_QUEUE = 65535;

enum Enum_ThreadPool_Shutdown
{
    IMMEDIATE_SHUTDOWN = 1,
    GRACEFUL_SHUTDOWN = 2
};

/// @brief 线程池任务
// @var function Pointer to the function that will perform the task.
// @var argument Argument to be passed to the function.
struct Stu_ThreadPoolTask
{
    // void (*function)(void *);
    // void *argument;
    std::function<void(std::shared_ptr<void>)> func;
    std::shared_ptr<void> args;
};

void myHandler(std::shared_ptr<void> req);

class ThreadPool
{
private:
    static pthread_mutex_t lock;
    static pthread_cond_t notify;
    static std::vector<pthread_t> threads;
    static std::vector<Stu_ThreadPoolTask> queue;
    static int thread_count; // 线程的数量
    static int queue_size;   // 队列最大数量
    static int head;         // 队头
    static int tail;         // 队尾
    static int count;        // 队列当前大小
    static int shutdown;     // 线程池是否关闭
    static int started;      // 活着的线程数量
public:
    static int threadPool_create(int thread_count, int queue_size);
    static int threadPool_add(std::shared_ptr<void> args, std::function<void(std::shared_ptr<void>)> fun = myHandler);
    // static int threadPool_destroy(ThreadPool *pool, int flags);
    // static int threadPool_free(ThreadPool *pool);
    // 工作线程
    static void *threadPool_thread(void *args);
};

#endif // THREADPOOL_H_