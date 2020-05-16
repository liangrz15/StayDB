#pragma once

#include <staydb/const.h>
#include <staydb/lockmanager/waitqueue.h>
#include <map>
#include <log4cplus/loggingmacros.h>
#include <log4cplus/logger.h>

class WaitQueueTable{
public:
    WaitQueueTable();
    void insert(uint ID);
    void wait(uint ID);
    void release(uint ID);

private:
    log4cplus::Logger logger;
    pthread_rwlock_t rwlock;
    std::map<uint, WaitQueue*> queues;

};