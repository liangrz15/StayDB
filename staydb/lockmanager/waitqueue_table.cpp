#include <staydb/lockmanager/waitqueue_table.h>

WaitQueueTable::WaitQueueTable(){
    pthread_rwlock_init(&rwlock, 0);
}

void WaitQueueTable::insert(uint ID){
    pthread_rwlock_wrlock(&rwlock);
    queues[ID] = new WaitQueue();
    pthread_rwlock_unlock(&rwlock);
}

void WaitQueueTable::wait(uint ID){
    pthread_rwlock_rdlock(&rwlock);
    WaitQueue* wait_queue = queues[ID];
    wait_queue->book();
    pthread_rwlock_unlock(&rwlock);
    wait_queue->wait();
}

void WaitQueueTable::release(uint ID){
    pthread_rwlock_wrlock(&rwlock);
    WaitQueue* wait_queue = queues[ID];
    queues.erase(ID);
    pthread_rwlock_unlock(&rwlock);
    wait_queue->release();
}
