#ifndef MUTEXLOCKGUARD_H_
#define MUTEXLOCKGUARD_H_

#include <pthread.h>

class MutexLockGuard
{
private:
    /* data */
public:
    explicit MutexLockGuard(/* args */);
    ~MutexLockGuard();

private: //禁用拷贝构造 和 拷贝赋值
    MutexLockGuard(const MutexLockGuard&) = delete;
    MutexLockGuard& operator=(const MutexLockGuard&) = delete;

private:
    static pthread_mutex_t lock;
};

#endif // MUTEXLOCKGUARD_H_