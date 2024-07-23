#ifndef TIMERMANAGER_H_
#define TIMERMANAGER_H_
#include "MyTimer.h"
#include "MutexLockGuard.hpp"

#include <queue>
#include <memory>

class RequestData;


struct TimerCmp
{
   bool operator()(std::shared_ptr<MyTimer> &a,std::shared_ptr<MyTimer> &b) const;
};

class TimerManager
{
    // typedef MyTimer TimerNode;
    typedef std::shared_ptr<RequestData> SP_ReqData;
    typedef std::shared_ptr<MyTimer> SP_TimerNode;
private:
    std::priority_queue<SP_TimerNode,std::deque<SP_TimerNode>,TimerCmp> TimerNodeQueue;
    MutexLock lock;

public:
    TimerManager(/* args */);
    ~TimerManager();
    void addTimer(SP_ReqData request_data,int timeout);
    void addTimer(SP_TimerNode timer_node);
    void handle_expired_event();
};



#endif  //TIMERMANAGER_H_