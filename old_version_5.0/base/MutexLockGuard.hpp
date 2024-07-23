#ifndef MUTEXLOCKGUARD_H_
#define MUTEXLOCKGUARD_H_
#include "MutexLock.hpp"
#include <pthread.h>

class MutexLockGuard : public NonCoypable
{
private:
    /* data */
public:
    explicit MutexLockGuard(MutexLock &_mutex) : mutex(_mutex)
    {
        mutex.lock();
    }
    ~MutexLockGuard()
    {
        mutex.unlock();
    }

private:
    MutexLock &mutex; //如果一个成员变量是引用类型，它必须在构造函数初始化列表中被初始化，
};



#endif // MUTEXLOCKGUARD_H_