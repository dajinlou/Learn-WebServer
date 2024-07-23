#ifndef THREADPOOL_H_
#define THREADPOOL_H_

#include <pthread.h>

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
struct Stu_ThreadPool_Task
{
    void (*function)(void *);
    void *argument;
};

struct ThreadPool
{
    pthread_mutex_t lock;
    pthread_cond_t notify;
    pthread_t *threads;
    Stu_ThreadPool_Task *queue;

    int thread_count; // 线程的数量
    int queue_size;   // 队列最大数量
    int head;         // 队头
    int tail;         // 队尾
    int count;        // 队列当前大小
    int shutdown;     // 线程池是否关闭
    int started;      // 活着的线程数量
};

ThreadPool *threadPool_create(int thread_count, int queue_size, int flags);
int threadPool_add(ThreadPool *pool, void (*function)(void *), void *argument, int flags);
int threadPool_destroy(ThreadPool *pool, int flags);
int threadPool_free(ThreadPool *pool);

// 工作线程
static void *threadPool_thread(void *threadpool);

#endif // THREADPOOL_H_