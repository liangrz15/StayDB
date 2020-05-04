#include <staydb/lockmanager/lockmanager.h>
#include <pthread.h>
#include <cassert>

LockManager* LockManager::instance = nullptr;

LockManager::LockManager(uint max_transaction_ID, uint max_timestamp){
    max_allocated_transaction_ID = max_transaction_ID;
    max_allocated_timestamp = max_timestamp;
    pthread_rwlock_init(&timestamp_rwlock, 0);
    pthread_mutex_init(&transaction_ID_mutex, 0);
    pthread_mutex_init(&commit_mutex, 0);
}

LockManager* LockManager::get_instance(){
    if(instance == nullptr){
        assert(0);
    }
    return instance;
}

void LockManager::create_instance(uint max_transaction_ID, uint max_timestamp){
    instance = new LockManager(max_transaction_ID, max_timestamp);
}


void LockManager::index_lock(const std::string& key){
    index_locktable.lock(key);
}

void LockManager::index_unlock(const std::string& key){
    index_locktable.unlock(key);
}

bool LockManager::key_write_lock(const std::string& key, uint static_transaction_ID, uint* occupier_transaction_ID){
    return key_write_locktable.occupy_lock(key, static_transaction_ID, occupier_transaction_ID);
}

void LockManager::key_write_unlock(const std::string& key, uint static_transaction_ID){
    key_write_locktable.occupy_unlock(key, static_transaction_ID);
}

void LockManager::slot_lock(const std::string& hash){
    slot_locktable.lock(hash);
}

void LockManager::slot_unlock(const std::string& hash){
    slot_locktable.unlock(hash);
}

uint LockManager::get_read_timestamp(){
    uint read_timestamp = 0;
    pthread_rwlock_rdlock(&timestamp_rwlock);
    read_timestamp = max_allocated_timestamp;
    pthread_rwlock_unlock(&timestamp_rwlock);
    return read_timestamp;
}

uint LockManager::commit_lock_and_get_timestamp(){
    pthread_mutex_lock(&commit_mutex);
    uint write_timestamp = 0;
    pthread_rwlock_rdlock(&timestamp_rwlock);
    write_timestamp = max_allocated_timestamp + 1;
    pthread_rwlock_unlock(&timestamp_rwlock);
    return write_timestamp;
}

void LockManager::commit_unlock(){
    pthread_rwlock_wrlock(&timestamp_rwlock);
    max_allocated_timestamp += 1;
    pthread_rwlock_unlock(&timestamp_rwlock);
    pthread_mutex_unlock(&commit_mutex);
}

uint LockManager::get_dynamic_transaction_ID(){
    uint dynamic_transaction_ID = 0;
    pthread_mutex_lock(&transaction_ID_mutex);
    max_allocated_transaction_ID += 1;
    dynamic_transaction_ID = max_allocated_transaction_ID;
    pthread_mutex_unlock(&transaction_ID_mutex);
    return dynamic_transaction_ID;
}

void LockManager::begin_transaction(uint static_transaction_ID){
    wait_queue_table.insert(static_transaction_ID);
}

void LockManager::end_transaction(uint static_transaction_ID){
    wait_queue_table.release(static_transaction_ID);
}

void LockManager::wait_for_transaction(uint static_transaction_ID){
    wait_queue_table.wait(static_transaction_ID);
}

void LockManager::ban_transactions(){
    /* TODO */
}

void LockManager::allow_transactions(){
    /* TODO */
}