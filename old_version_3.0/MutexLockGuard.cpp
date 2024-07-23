#include "MutexLockGuard.h"

pthread_mutex_t MutexLockGuard::lock = PTHREAD_MUTEX_INITIALIZER;
MutexLockGuard::MutexLockGuard(/* args */)
{
    pthread_mutex_lock(&lock);
}

MutexLockGuard::~MutexLockGuard()
{
    pthread_mutex_unlock(&lock);
}