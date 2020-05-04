#pragma once
#include <pthread.h>

class WaitQueue{
public:
    WaitQueue();
    void book();
    void wait();
    void release();

private:
    int n_booked;
    int n_waiters;
    bool released;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    bool safe_to_delete();
};