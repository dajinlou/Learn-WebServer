#ifndef MYTIMER_H_
#define MYTIMER_H_

#include <string.h>
#include <memory>


class RequestData;

class MyTimer
{
public:
    MyTimer(std::shared_ptr<RequestData> _requset_data,int timeout);
    ~MyTimer();

    //更新定时器
    void update(int timeout);
    //判断定时器是否有效
    bool isvalid();
    //清除
    void clearReq();

    //设置删除
    void setDeleted();

    //是否被删除
    bool isDeleted() const;

    //获取过期时间
    size_t getExpTime() const;

public:
    bool deleted;
    size_t expired_time;
    std::shared_ptr<RequestData> request_data;
};


#endif //MYTIMER_H_