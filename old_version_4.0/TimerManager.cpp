#include "TimerManager.h"
#include "RequestData.h"

TimerManager::TimerManager(/* args */)
{
}

TimerManager::~TimerManager()
{
}
void TimerManager::addTimer(SP_ReqData request_data, int timeout)
{
    SP_TimerNode new_node(new MyTimer(request_data, timeout));
    {
        MutexLockGuard locker(lock);
        TimerNodeQueue.push(new_node);
    }
    // TODO: linkTimer函数功能的实现
    request_data->linkTimer(new_node);
}
void TimerManager::addTimer(SP_TimerNode timer_node)
{
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
void TimerManager::handle_expired_event()
{
    // pthread_mutex_lock(&qlock);
    MutexLockGuard locker(lock);
    while (!TimerNodeQueue.empty())
    {
        SP_TimerNode ptimer_now = TimerNodeQueue.top();
        if (ptimer_now->isDeleted())
        {
            TimerNodeQueue.pop();
            // delete ptimer_now;
        }
        else if (ptimer_now->isvalid() == false)
        {
            TimerNodeQueue.pop();
            // delete ptimer_now;
        }
        else
        {
            break;
        }
    }
}

bool TimerCmp::operator()(std::shared_ptr<MyTimer> &a, std::shared_ptr<MyTimer> &b) const
{
    return a->getExpTime() > b->getExpTime();
}