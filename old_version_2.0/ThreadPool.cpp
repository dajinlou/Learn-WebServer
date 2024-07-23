#include "ThreadPool.h"

#include <stdlib.h>

ThreadPool *threadPool_create(int thread_count, int queue_size, int flags)
{
    ThreadPool *pool;
    int i;
    do
    {
        //1 检测参数的合法性
        if (thread_count <= 0 || thread_count > MAX_THREADS || queue_size <= 0 || queue_size > MAX_QUEUE)
        {
            return NULL;
        }

        if ((pool = (ThreadPool *)malloc(sizeof(ThreadPool))) == NULL)
        {
            break;
        }

        //2 初始化
        pool->thread_count = 0;
        pool->queue_size = queue_size;
        pool->head = pool->tail = pool->count = 0;
        pool->shutdown = pool->started = 0;

        //3 分配线程和任务队列内存
        pool->threads = (pthread_t *)malloc(sizeof(pthread_t) * thread_count);
        pool->queue = (Stu_ThreadPool_Task *)malloc(sizeof(Stu_ThreadPool_Task) * queue_size);

        //4 初始化 mutex 和 条件变量
        if ((pthread_mutex_init(&(pool->lock), NULL) != 0) ||
            (pthread_cond_init(&(pool->notify), NULL) != 0) ||
            (pool->threads == NULL) ||
            (pool->queue == NULL))
        {
            break;
        }

        //5 开启工作线程
        for (int i = 0; i < thread_count; ++i)
        {
            if (pthread_create(&(pool->threads[i]), NULL, threadPool_thread, (void *)pool) != 0)
            {
                threadPool_destroy(pool, 0);
                return NULL;
            }
            pool->thread_count++;
            pool->started++;
        }
        return pool;

    } while (false);

    if (NULL != pool)
    {
        threadPool_free(pool);
    }
    return NULL;
}

int threadPool_add(ThreadPool *pool, void (*function)(void *), void *argument, int flags)
{
    int err = 0;
    int next;
    if (pool == NULL || function == NULL)
    {
        return THREADPOOL_INVALID;
    }
    if (pthread_mutex_lock(&(pool->lock)) != 0)
    {
        return THREADPOOL_LOCK_FAILURE;
    }
    next = (pool->tail + 1) % pool->queue_size;
    do
    {
        // 是否满
        if (pool->count == pool->queue_size)
        {
            err = THREADPOOL_QUEUE_FULL;
        }
        if (pool->shutdown)
        {
            err = THREADPOOL_SHUTDOWN;
            break;
        }
        // 添加任务到队列中
        pool->queue[pool->tail].function = function;
        pool->queue[pool->tail].argument = argument;
        pool->tail = next;
        pool->count += 1;

        // 通过广播通知
        if (pthread_cond_signal(&(pool->notify)) != 0)
        {
            err = THREADPOOL_LOCK_FAILURE;
            break;
        }
    } while (false);
    if (pthread_mutex_unlock(&pool->lock) != 0)
    {
        err = THREADPOOL_LOCK_FAILURE;
    }

    return err;
}

int threadPool_destroy(ThreadPool *pool, int flags)
{
    int i;
    int err = 0;
    if (pool == NULL)
    {
        return THREADPOOL_INVALID;
    }
    if (pthread_mutex_lock(&(pool->lock)) != 0)
    {
        return THREADPOOL_LOCK_FAILURE;
    }
    do
    {
        // 已经关闭
        if (pool->shutdown)
        {
            err = THREADPOOL_SHUTDOWN;
            break;
        }
        pool->shutdown = (flags & THREADPOOL_GRACEFUL) ? GRACEFUL_SHUTDOWN : IMMEDIATE_SHUTDOWN;

        // 唤醒所有工作线程
        if ((pthread_cond_broadcast(&(pool->notify)) != 0) ||
            (pthread_mutex_unlock(&(pool->lock)) != 0))
        {
            err = THREADPOOL_LOCK_FAILURE;
            break;
        }

        // join 所有工作线程
        for (int i = 0; i < pool->thread_count; ++i)
        {
            if (pthread_join(pool->threads[i], NULL) != 0)
            {
                err = THREADPOOL_THREAD_FAILURE;
            }
        }

    } while (false);

    //  直到每个都完成，我们关闭线程池
    if (!err)
    {
        threadPool_free(pool);
    }

    return err;
}

int threadPool_free(ThreadPool *pool)
{
    if (NULL == pool || pool->started > 0)
        return -1;
    if (pool->threads)
    {
        free(pool->threads);
        free(pool->queue);
        pthread_mutex_lock(&(pool->lock));
        pthread_mutex_destroy(&(pool->lock));
        pthread_cond_destroy(&(pool->notify));
    }
    free(pool);
    return 0;
}

void * threadPool_thread(void *threadpool)
{
    // 拿到线程池
    ThreadPool *pool = (ThreadPool *)threadpool;
    Stu_ThreadPool_Task task;
    for (;;)
    {
        pthread_mutex_lock(&(pool->lock));

        // 用于检测虚假的唤醒
        while ((pool->count == 0) && (!pool->shutdown))
        {
            pthread_cond_wait(&(pool->notify), &(pool->lock));
        }
        if ((pool->shutdown == IMMEDIATE_SHUTDOWN) ||
            ((pool->shutdown == GRACEFUL_SHUTDOWN) && (pool->count)))
        {
            break;
        }

        // 取出任务
        task.function = pool->queue[pool->head].function;
        task.argument = pool->queue[pool->head].argument;
        pool->head = (pool->head + 1) % pool->queue_size;
        pool->count -= 1;

        // 解锁
        pthread_mutex_unlock(&(pool->lock));

        // 进行工作
        //  (*(task.function))(task.argument);
        //其中function是 myHandler  而 argument是RequesetData对象
        task.function(task.argument);
    }
    --pool->started;
    pthread_mutex_unlock(&(pool->lock));
    pthread_exit(NULL);
    return NULL;
}
