#include <staydb/lockmanager/occupy_locktable.h>

OccupyLockTable::OccupyLockTable(){
    pthread_mutex_init(&mutex, 0);
}

bool OccupyLockTable::occupy_lock(const std::string& key, uint applicant_ID, uint applicant_dynamic_ID, uint* occupier_ID){
    pthread_mutex_lock(&mutex);
    if(occupy_locks.find(key) == occupy_locks.end()){
        occupy_locks[key] = OccupyLock(true, applicant_ID, applicant_dynamic_ID);
        pthread_mutex_unlock(&mutex);
        return true;
    }
    OccupyLock& lock = occupy_locks[key];
    if(lock.occupied == false){
        occupy_locks[key] = OccupyLock(true, applicant_ID, applicant_dynamic_ID);
        pthread_mutex_unlock(&mutex);
        return true;
    }
    uint occupier_dynamic_ID = lock.occupier_dynamic_ID;
    if(aborting_dynamic_IDs.find(occupier_dynamic_ID) == aborting_dynamic_IDs.end()){
        aborting_dynamic_IDs.insert(applicant_dynamic_ID);
        wait_queue_table.insert(applicant_dynamic_ID);
        *occupier_ID = lock.occupier_ID;
        pthread_mutex_unlock(&mutex);
        return false;
    }
    occupy_locks[key] = OccupyLock(true, applicant_ID, applicant_dynamic_ID);
    pthread_mutex_unlock(&mutex);
    wait_queue_table.wait(occupier_dynamic_ID);
    return true;
}

void OccupyLockTable::occupy_unlock(const std::string& key, uint applicant_ID){
    pthread_mutex_lock(&mutex);
    assert(occupy_locks.find(key) != occupy_locks.end());
    OccupyLock& lock = occupy_locks[key];
    if(lock.occupier_ID == applicant_ID){
        occupy_locks[key].occupied = false;
    }
    pthread_mutex_unlock(&mutex);
}

void OccupyLockTable::occupy_abort_finish(uint applicant_dynamic_ID){
    pthread_mutex_lock(&mutex);
    aborting_dynamic_IDs.erase(applicant_dynamic_ID);
    wait_queue_table.release(applicant_dynamic_ID);
    pthread_mutex_unlock(&mutex);
}