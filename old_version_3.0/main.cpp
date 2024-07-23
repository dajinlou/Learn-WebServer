#include "Epoll.h"
#include "ThreadPool.h"
#include "RequestData.h"
#include "Util.h"
#include "MyTimer.h"
#include "MutexLockGuard.h"

#include <iostream>
#include <string>
#include <queue>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <memory>

using namespace std;

const int THREADPOOL_THREAD_NUM = 4;
const int QUEUE_SIZE = 65535;

const int MAXEVENTS = 5000;
const int LISTENQ = 1024;

const int PORT = 9527;
const int ASK_STATIC_FILE = 1;
const int ASK_IMAGE_STITCH = 2; //

const std::string PATH = "/";

const int TIMER_TIME_OUT = 500;
// extern 是一个关键字，用于声明一个变量或函数是在另一个文件中定义的，但在当前文件中是外部可访问的。这通常用于在多个文件之间共享全局变量或函数。
// extern pthread_mutex_t qlock;   //进行改进 之前在RequestData的头文件中定义
extern struct epoll_event *events;
extern priority_queue<shared_ptr<MyTimer>, deque<shared_ptr<MyTimer>>, TimerCmp> myTimerQueue;

int socket_bind_listen(int port)
{
    // 检测port值
    if (port < 1024 || port > 65535)
    {
        return -1;
    }

    int lfd = socket(AF_INET, SOCK_STREAM, 0);
    if (lfd == -1)
    {
        perror("socket failed...");
        exit(1);
    }

    // 设置 地址复用
    int opt = 1;
    if (setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) != 0)
    {
        perror("setsockopt failed...");
        exit(1);
    }

    // 绑定地址
    struct sockaddr_in serv_addr;
    bzero((char *)&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(port);
    int res = bind(lfd, (sockaddr *)&serv_addr, sizeof(serv_addr));
    if (res == -1)
    {
        perror("bind failed...");
        exit(1);
    }
    res = listen(lfd, 128);
    if (res == -1)
    {
        perror("listen failed...");
        exit(1);
    }
    if (lfd == -1)
    {
        close(lfd);
        return -1;
    }

    return lfd;
}

/* 处理逻辑是这样的~
因为
(1) 优先队列不支持随机访问
(2) 即使支持，随机删除某节点后破坏了堆的结构，需要重新更新堆结构。
所以对于被置为deleted的时间节点，会延迟到它
(1)超时 或
(2)它前面的节点都被删除时，它才会被删除。
一个点被置为deleted,它最迟会在TIMER_TIME_OUT时间后被删除。
这样做有两个好处：
(1) 第一个好处是不需要遍历优先队列，省时。
(2) 第二个好处是给超时时间一个容忍的时间，就是设定的超时时间是删除的下限(并不是一到超时时间就立即删除)，如果监听的请求在超时后的下一次请求中又一次出现了，
就不用再重新申请requestData节点了，这样可以继续重复利用前面的requestData，减少了一次delete和一次new的时间。
*/
void handle_expired_event()
{
    // pthread_mutex_lock(&qlock);
    MutexLockGuard();
    while (!myTimerQueue.empty())
    {
        shared_ptr<MyTimer> ptimer_now = myTimerQueue.top();
        if (ptimer_now->isDeleted())
        {
            myTimerQueue.pop();
            // delete ptimer_now;
        }
        else if (ptimer_now->isvalid() == false)
        {
            myTimerQueue.pop();
            // delete ptimer_now;
        }
        else
        {
            break;
        }
    }
}

int main()
{
    // 忽视管道中断信号
    handle_for_sigpipe();

    // 创建树根
    if (Epoll::epoll_init(MAXEVENTS, LISTENQ) < 0)
    {
        perror("epoll init failed");
        return 1;
    }
    if(ThreadPool::threadPool_create(THREADPOOL_THREAD_NUM, QUEUE_SIZE)<0){
        printf("ThreadPool create failed\n");
        return 1;
    }
   
    int listen_fd = socket_bind_listen(PORT);
    if (listen_fd < 0)
    {
        perror("socket bind failed");
        return 1;
    }
    if (setSocketNonBlocking(listen_fd) < 0)
    {
        perror("set socket non block failed");
        return 1;
    }
    shared_ptr<RequestData> req(new RequestData());
    req->setFd(listen_fd);
    if (Epoll::epoll_add(listen_fd, req, EPOLLIN | EPOLLET) < 0)
    {
        perror("epoll add error");
        return 1;
    }
    while (true)
    {
        Epoll::my_epoll_wait(listen_fd, MAXEVENTS, -1);
        handle_expired_event();
    }

    return 0;
}
