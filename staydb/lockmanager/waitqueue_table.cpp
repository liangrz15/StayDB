#include <staydb/lockmanager/waitqueue_table.h>

WaitQueueTable::WaitQueueTable(){
    pthread_rwlock_init(&rwlock, 0);
    logger = log4cplus::Logger::getInstance(LOG4CPLUS_TEXT("WaitQueueTable"));
}

void WaitQueueTable::insert(uint ID){
    pthread_rwlock_wrlock(&rwlock);
    LOG4CPLUS_DEBUG(logger, LOG4CPLUS_TEXT("WaitQueueTable::insert: ") << ID);
    queues[ID] = new WaitQueue();
    pthread_rwlock_unlock(&rwlock);
}

void WaitQueueTable::wait(uint ID){
    pthread_rwlock_rdlock(&rwlock);
    LOG4CPLUS_DEBUG(logger, LOG4CPLUS_TEXT("WaitQueueTable::wait: ") << ID);
    if(queues.find(ID) == queues.end()){
        pthread_rwlock_unlock(&rwlock);
        return;
    }
    WaitQueue* wait_queue = queues[ID];
    wait_queue->book();
    pthread_rwlock_unlock(&rwlock);
    wait_queue->wait();
}

void WaitQueueTable::release(uint ID){
    pthread_rwlock_wrlock(&rwlock);
    LOG4CPLUS_DEBUG(logger, LOG4CPLUS_TEXT("WaitQueueTable::release: ") << ID);
    WaitQueue* wait_queue = queues[ID];
    queues.erase(ID);
    pthread_rwlock_unlock(&rwlock);
    wait_queue->release();
}
