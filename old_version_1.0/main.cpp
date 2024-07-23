#include "Epoll.h"
#include "ThreadPool.h"
#include "RequestData.h"
#include "Util.h"
#include "MyTimer.h"

#include <iostream>
#include <string>
#include <queue>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>

using namespace std;

const int THREADPOOL_THREAD_NUM = 4;
const int QUEUE_SIZE = 65535;

const int PORT = 9527;
const int ASK_STATIC_FILE = 1;
const int ASK_IMAGE_STITCH = 2; //

const std::string PATH = "/";

const int TIMER_TIME_OUT = 500;
// extern 是一个关键字，用于声明一个变量或函数是在另一个文件中定义的，但在当前文件中是外部可访问的。这通常用于在多个文件之间共享全局变量或函数。
extern pthread_mutex_t qlock;
extern struct epoll_event *events;
extern priority_queue<MyTimer *, deque<MyTimer *>, TimerCmp> myTimerQueue;

void acceptConnection(int listen_fd, int epoll_fd, const string &path)
{
    struct sockaddr_in client_addr;
    memset(&client_addr, 0, sizeof(struct sockaddr_in));
    socklen_t client_addr_len = 0;
    int accept_fd = 0;
    while ((accept_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &client_addr_len)) > 0)
    {
        /*
        // TCP的保活机制默认是关闭的
        int optval = 0;
        socklen_t len_optval = 4;
        getsockopt(accept_fd, SOL_SOCKET,  SO_KEEPALIVE, &optval, &len_optval);
        cout << "optval ==" << optval << endl;
        */

        // 设为非阻塞模式
        int ret = setSocketNonBlocking(accept_fd);
        if (ret < 0)
        {
            perror("Set non block failed!");
            return;
        }

        RequestData *req_info = new RequestData(epoll_fd, accept_fd, path);
        cout<<"新建立的客户端："<<accept_fd<<endl;
        // 文件描述符可以读，边缘触发(Edge Triggered)模式，保证一个socket连接在任一时刻只被一个线程处理
        __uint32_t _epo_event = EPOLLIN | EPOLLET | EPOLLONESHOT;
        epoll_add(epoll_fd, accept_fd, static_cast<void *>(req_info), _epo_event);
        // 新增时间信息
        MyTimer *mtimer = new MyTimer(req_info, TIMER_TIME_OUT);
        req_info->addTimer(mtimer);
        pthread_mutex_lock(&qlock);
        myTimerQueue.push(mtimer);
        pthread_mutex_unlock(&qlock);
    }
}

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

void myHandler(void *args)
{
    RequestData *req_data = (RequestData *)args;
    req_data->handleRequest();
}

// 分发处理函数
void handle_events(int epoll_fd, int listen_fd, struct epoll_event *events, int events_num, const string &path, ThreadPool *tp)
{
    for (int i = 0; i < events_num; ++i)
    {
        // 获取有时间产生的描述符
        RequestData *request = (RequestData *)(events[i].data.ptr);
        int fd = request->getFd();

        // 有事件产生的描述符为监听描述符
        if (fd == listen_fd)
        {
            acceptConnection(listen_fd, epoll_fd, path);
        }
        else
        { // 排除错误事件
            if ((events[i].events & EPOLLERR) || (events[i].events & EPOLLHUP) || (!(events[i].events & EPOLLIN)))
            {
                printf("error event\n");
                delete request;
                continue;
            }

            // 将请求任务加入到线程池中
            // 加入线程池之前将Timer和request分离
            request->seperateTimer();
            int rc = threadPool_add(tp, myHandler, events[i].data.ptr, 0);
        }
    }
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
    pthread_mutex_lock(&qlock);
    while (!myTimerQueue.empty())
    {
        MyTimer *ptimer_now = myTimerQueue.top();
        if (ptimer_now->isDeleted())
        {
            myTimerQueue.pop();
            delete ptimer_now;
        }
        else if (ptimer_now->isvalid() == false)
        {
            myTimerQueue.pop();
            delete ptimer_now;
        }
        else
        {
            break;
        }
    }
    pthread_mutex_unlock(&qlock);
}

int main()
{
    // 忽视管道中断信号
    handle_for_sigpipe();

    // 创建树根
    int epoll_fd = epoll_init();
    if (epoll_fd < 0)
    {
        perror("epoll init failed");
        return 1;
    }

    ThreadPool *threadPool = threadPool_create(THREADPOOL_THREAD_NUM, QUEUE_SIZE, 0);
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
    __uint32_t event = EPOLLIN | EPOLLET;
    RequestData *req = new RequestData();
    req->setFd(listen_fd);
    epoll_add(epoll_fd, listen_fd, static_cast<void *>(req), event);
    while (true)
    {
        int events_num = my_epoll_wait(epoll_fd, events, MAXEVENTS, -1);
        // printf("%zu\n", myTimerQueue.size());
        if (events_num == 0)
            continue;
        printf("事件数量：%d\n", events_num);
        // printf("%zu\n", myTimerQueue.size());
        //  else
        //      cout << "one connection has come!" << endl;
        //  遍历events数组，根据监听种类及描述符类型分发操作
        handle_events(epoll_fd, listen_fd, events, events_num, PATH, threadPool);

        handle_expired_event();
    }

    return 0;
}
