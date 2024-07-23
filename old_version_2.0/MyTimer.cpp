#include "MyTimer.h"
#include "RequestData.h"
#include <sys/time.h>
#include <iostream>

MyTimer::MyTimer(RequestData *_requset_data, int timeout)
    : deleted(false),
      request_data(_requset_data)
{
    struct timeval now;
    gettimeofday(&now,NULL);
    // 以毫秒计
    expired_time = ((now.tv_sec * 1000) + (now.tv_usec / 1000)) + timeout;
}

MyTimer::~MyTimer()
{
#if DEBUG
    std::cout << "~mytimer()" << std::endl;
#endif
    if(request_data != NULL){
        std::cout << "request_data = "<< request_data <<" cfd:"<<request_data->getFd()<< std::endl;
        delete request_data;
        request_data = NULL;
    }
}

void MyTimer::update(int timeout)
{
    struct timeval now;
    gettimeofday(&now,NULL);
    expired_time = ((now.tv_sec*1000)+(now.tv_usec/1000))+timeout;
}

bool MyTimer::isvalid()
{
    struct timeval now;
    gettimeofday(&now,NULL);
    size_t temp = ((now.tv_sec*1000)+(now.tv_usec/1000));
    if(temp < expired_time){
        return true;
    }
    else{
        this->setDeleted();
        return false;
    }
}

void MyTimer::clearReq()
{
    request_data = NULL;
    this->setDeleted();
}

void MyTimer::setDeleted()
{
    deleted = true;
}

bool MyTimer::isDeleted() const
{
    return deleted;
}

size_t MyTimer::getExpTime() const
{
    return expired_time;
}

