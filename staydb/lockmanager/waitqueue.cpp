#include <staydb/lockmanager/waitqueue.h>

WaitQueue::WaitQueue(): n_booked(0), n_waiters(0), released(false) {
    pthread_mutex_init(&mutex, 0);
    pthread_cond_init(&cond, 0);
}

void WaitQueue::book(){
    pthread_mutex_lock(&mutex);
    n_booked += 1;
    pthread_mutex_unlock(&mutex);
}

void WaitQueue::wait(){
    pthread_mutex_lock(&mutex);
    n_booked -= 1;
    if(!released){
        n_waiters += 1;
        pthread_cond_wait(&cond, &mutex);
        n_waiters -= 1;
    }
    bool safe = safe_to_delete();
    pthread_mutex_unlock(&mutex);
    if(safe){
        delete this;
    }
}

void WaitQueue::release(){
    pthread_mutex_lock(&mutex);
    released = true;
    bool safe = safe_to_delete();
    pthread_cond_broadcast(&cond);
    pthread_mutex_unlock(&mutex);
    if(safe){
        delete this;
    }
}

bool WaitQueue::safe_to_delete(){
    return released && (n_booked == 0) && (n_waiters == 0);
}