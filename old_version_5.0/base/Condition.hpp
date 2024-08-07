#ifndef CONDITION_H_
#define CONDITION_H_
#include "MutexLock.hpp"

class Condition
{
private:
    MutexLock &mutex;
    pthread_cond_t cond;
public:
    explicit Condition(MutexLock &_mutex):mutex(_mutex)
    {
        pthread_cond_init(&cond,NULL);
    }
    ~Condition(){
        pthread_cond_destroy(&cond);
    }

    void wait()
    {
        pthread_cond_wait(&cond,mutex.get());
    }
    void notify()
    {
        pthread_cond_signal(&cond);
    }
    void notifyAll()
    {
        pthread_cond_broadcast(&cond);
    }
};

#endif // CONDITION_H_