#pragma once
#include <string>
#include <map>
#include <pthread.h>
#include <set>
#include <staydb/lockmanager/waitqueue_table.h>

struct OccupyLock{
    OccupyLock(bool _occupied = false, uint _occupier_ID = 0, uint _occupier_dynamic_ID = 0): occupied(_occupied), occupier_ID(_occupier_ID),
                occupier_dynamic_ID(_occupier_dynamic_ID) {}
    bool occupied;
    uint occupier_ID;
    uint occupier_dynamic_ID;
};

class OccupyLockTable{
public:
    OccupyLockTable();
    bool occupy_lock(const std::string& key, uint applicant_ID, uint applicant_dynamic_ID, uint* occupier_ID);
    void occupy_unlock(const std::string& key, uint applicant_ID);
    void occupy_abort_finish(uint applicant_dynamic_ID);

private:
    pthread_mutex_t mutex;
    std::map<std::string, OccupyLock> occupy_locks;
    std::set<uint> aborting_dynamic_IDs;
    WaitQueueTable wait_queue_table;
};