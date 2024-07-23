#ifndef MYTIMER_H_
#define MYTIMER_H_

#include <string.h>
#include <memory>


class RequestData;

class MyTimer
{
    typedef std::shared_ptr<RequestData> SP_ReqData;
public:
    MyTimer(SP_ReqData _requset_data,int timeout);
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
    SP_ReqData request_data;
};


#endif //MYTIMER_H_