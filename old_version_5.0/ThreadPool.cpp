#include "ThreadPool.h"
#include "RequestData.h"
#include "Debug.h"

pthread_mutex_t ThreadPool::lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t ThreadPool::notify = PTHREAD_COND_INITIALIZER;
std::vector<pthread_t> ThreadPool::threads;
std::vector<Stu_ThreadPoolTask> ThreadPool::queue;
int ThreadPool::thread_count = 0;
int ThreadPool::queue_size = 0;
int ThreadPool::head = 0;
int ThreadPool::tail = 0;
int ThreadPool::count = 0;
int ThreadPool::shutdown = 0;
int ThreadPool::started = 0;

void myHandler(std::shared_ptr<void> req)
{
    std::shared_ptr<RequestData> request = std::static_pointer_cast<RequestData>(req);
    if(request->canWrite()){
        request->handleWrite();
    }
    else if(request->canRead()){
        request->handleRead();
    }
    request->handleConn();
}

int ThreadPool::threadPool_create(int _thread_count, int _queue_size)
{
    bool err = false;
    do
    {
        //1 检测参数的合法性
        if (_thread_count <= 0 || _thread_count > MAX_THREADS || _queue_size <= 0 || _queue_size > MAX_QUEUE)
        {
            _thread_count = 4;
            _queue_size = 1024;
        }

        //2 初始化
        thread_count = 0;
        queue_size = _queue_size;
        head = tail = count = 0;
        shutdown = started = 0;

        //3 分配线程和任务队列内存
        threads.resize(_thread_count);
        queue.resize(_queue_size);


        //5 开启工作线程
        for (int i = 0; i < _thread_count; ++i)
        {
            if (pthread_create(&threads[i], NULL, threadPool_thread, (void *)(0)) != 0)
            {
                return -1;
            }
            thread_count++;
            started++;
        }
    } while (false);

    if (err)
    {
        return -1;
    }
    return 0;
}

int ThreadPool::threadPool_add(std::shared_ptr<void> args, std::function<void(std::shared_ptr<void>)> fun)
{
    int err = 0;
    int next;
    if (pthread_mutex_lock(&lock) != 0)
    {
        return THREADPOOL_LOCK_FAILURE;
    }
    
    do
    {
        next = (tail + 1) % queue_size;
        // 是否满
        if (count == queue_size)
        {
            err = THREADPOOL_QUEUE_FULL;
            break;
        }
        if (shutdown)
        {
            err = THREADPOOL_SHUTDOWN;
            break;
        }
        // 添加任务到队列中
        queue[tail].func = fun;
        queue[tail].args = args;
        tail = next;
        count += 1;

        // 通过广播通知
        if (pthread_cond_signal(&notify) != 0)
        {
            err = THREADPOOL_LOCK_FAILURE;
            break;
        }
    } while (false);
    if (pthread_mutex_unlock(&lock) != 0)
    {
        err = THREADPOOL_LOCK_FAILURE;
    }

    return err;
}


void * ThreadPool::threadPool_thread(void *args)
{
   
    while (true)
    {
        Stu_ThreadPoolTask task;
        pthread_mutex_lock(&lock);

        // 用于检测虚假的唤醒
        while ((count == 0) && (!shutdown))
        {
            debug()<<"等待信号..."<<endl;
            pthread_cond_wait(&notify, &lock);
        }
        if ((shutdown == IMMEDIATE_SHUTDOWN) ||
            ((shutdown == GRACEFUL_SHUTDOWN) && (count==0)))
        {
            break;
        }
        debug()<<"执行任务.........."<<endl;
        // 取出任务
        task.func = queue[head].func;
        task.args = queue[head].args;
        queue[head].func = NULL;
        queue[head].args.reset(); //重置
        head = (head + 1) % queue_size;
        count -= 1;

        // 解锁
        pthread_mutex_unlock(&lock);

        // 进行工作
        //  (*(task.function))(task.argument);
        //其中function是 myHandler  而 argument是RequesetData对象
        task.func(task.args);
    }
    --started;
    pthread_mutex_unlock(&lock);
    pthread_exit(NULL);
    return (NULL);
}
