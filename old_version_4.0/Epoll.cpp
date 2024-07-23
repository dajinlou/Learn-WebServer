#include "Epoll.h"
#include "MutexLockGuard.hpp"
#include "MyTimer.h"
#include "Util.h"
#include "ThreadPool.h"
#include "Debug.h"

#include <sys/epoll.h>
#include <queue>
#include <deque>
#include <arpa/inet.h>
#include <iostream>

using namespace std;

int TIMER_TIME_OUT = 500;

epoll_event *Epoll::events;
std::unordered_map<int, std::shared_ptr<RequestData>> Epoll::fd2Req;
int Epoll::epoll_fd = 0;
const std::string Epoll::PATH = "/";

TimerManager Epoll::timer_manager;


Epoll::Epoll(/* args */)
{
}

Epoll::~Epoll()
{
}
int Epoll::epoll_init(int maxevents, int listen_num)
{
    epoll_fd = epoll_create(listen_num + 1);
    if (-1 == epoll_fd)
    {
        return -1;
    }
    events = new epoll_event[maxevents];

    return 0;
}

// 注册描述符
int Epoll::epoll_add(int fd, std::shared_ptr<RequestData> request, __uint32_t events)
{
    struct epoll_event event;
    // event.data.ptr = request;
    event.data.fd = fd;
    event.events = events;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &event) < 0)
    {
        perror("epoll_add error");
        return -1;
    }
    fd2Req[fd] = request;
    return 0;
}

// 修改描述符状态
int Epoll::epoll_mod(int fd, std::shared_ptr<RequestData> request, __uint32_t events)
{
    struct epoll_event event;
    event.data.fd = fd;
    event.events = events;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &event) < 0)
    {
        perror("epoll_mod error");
        return -1;
    }
    fd2Req[fd] = request;
    return 0;
}

// 从epoll中删除描述符
int Epoll::epoll_del(int fd, __uint32_t events)
{
    struct epoll_event event;
    event.data.fd = fd;
    event.events = events;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, &event) < 0)
    {
        perror("epoll_del error");
        return -1;
    }
    auto fd_it = fd2Req.find(fd);
    if (fd_it != fd2Req.end())
    {
        fd2Req.erase(fd_it);
    }

    return 0;
}

void Epoll::my_epoll_wait(int listen_fd, int max_events, int timeout)
{
    int event_count = epoll_wait(epoll_fd, events, max_events, timeout);
    if (event_count < 0)
    {
        perror("epoll wait error");
    }
    std::vector<std::shared_ptr<RequestData>> req_data = getEventsRequest(listen_fd, event_count, PATH);
    if(req_data.size() > 0){
        for(auto &req: req_data){
            if(ThreadPool::threadPool_add(req)<0){
                //线程池满了或者关闭了等原因，抛弃本次监听到的请求。
                break;
            }
        }
    }
    timer_manager.handle_expired_event();
}

void Epoll::acceptConnection(int listen_fd, int epoll_fd, const std::string &path)
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
        char client_ip[16]={0};
        // cout << inet_addr(client_addr.sin_addr.s_addr) << endl;
        inet_ntop(AF_INET,&(client_addr.sin_addr),client_ip,sizeof(client_ip));
        debug() << client_ip << endl;
        debug() << client_addr.sin_port << endl;

        // 设为非阻塞模式
        int ret = setSocketNonBlocking(accept_fd);
        if (ret < 0)
        {
            perror("Set non block failed!");
            return;
        }

        std::shared_ptr<RequestData> req_info(new RequestData(epoll_fd, accept_fd, path));

        // 文件描述符可以读，边缘触发(Edge Triggered)模式，保证一个socket连接在任一时刻只被一个线程处理
        __uint32_t _epo_event = EPOLLIN | EPOLLET | EPOLLONESHOT;
        Epoll::epoll_add(accept_fd, req_info, _epo_event);
        // 新增时间信息
        // std::shared_ptr<MyTimer> mtimer(new MyTimer(req_info, TIMER_TIME_OUT));
        // req_info->addTimer(mtimer);
        timer_manager.addTimer(req_info,TIMER_TIME_OUT);
    }
}

std::vector<std::shared_ptr<RequestData>> Epoll::getEventsRequest(int listen_fd, int events_num, const std::string path)
{
    std::vector<std::shared_ptr<RequestData>> req_data;
    for (int i = 0; i < events_num; ++i)
    {
        // 获取有事件产生得描述符
        int fd = events[i].data.fd;

        // 有事件发生得描述符为监听描述符
        if (fd == listen_fd)
        {
            acceptConnection(listen_fd, epoll_fd, path);
        }
        else if (fd < 3)
        {
            break;
        }
        else
        {
            // 排除错误事件
            if ((events[i].events & EPOLLERR) || (events[i].events & EPOLLHUP) || (!(events[i].events & EPOLLIN)))
            {
                auto fd_it = fd2Req.find(fd);
                if (fd_it != fd2Req.end())
                {
                    fd2Req.erase(fd_it); 
                }
                continue;
            }
                // 将请求任务加入到线程池中
                // 加入线程池之前将Timer和Request分离
                std::shared_ptr<RequestData> cur_req(fd2Req[fd]);
                cur_req->seperateTimer();
                req_data.push_back(cur_req);
                auto fd_ite = fd2Req.find(fd);
                if (fd_ite != fd2Req.end())
                {
                    fd2Req.erase(fd_ite);
                }      
        }
    }
    return req_data;
}

void Epoll::add_timer(SP_ReqData requestData, int timeout)
{
    timer_manager.addTimer(requestData,timeout);
}