#pragma once

#include <staydb/const.h>
#include <staydb/lockmanager/waitqueue.h>
#include <map>

class WaitQueueTable{
public:
    WaitQueueTable();
    void insert(uint ID);
    void wait(uint ID);
    void release(uint ID);

private:
    pthread_rwlock_t rwlock;
    std::map<uint, WaitQueue*> queues;

};