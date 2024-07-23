#ifndef MUTEXLOCK_H_
#define MUTEXLOCK_H_
#include "NonCoypable.hpp"

#include <pthread.h>

class MutexLock : NonCoypable
{
public:
    MutexLock(/* args */)
    {
        pthread_mutex_init(&mutex, NULL);
    }
    ~MutexLock()
    {
        pthread_mutex_lock(&mutex);
        pthread_mutex_destroy(&mutex);
    }

    void lock()
    {
        pthread_mutex_lock(&mutex);
    }

    void unlock()
    {
        pthread_mutex_unlock(&mutex);
    }

private:
    pthread_mutex_t mutex;
};

#endif // MUTEXLOCK_H_