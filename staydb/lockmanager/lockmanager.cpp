#include <staydb/lockmanager/lockmanager.h>
#include <pthread.h>
#include <cassert>

LockManager* LockManager::instance = nullptr;

LockManager::LockManager(uint max_transaction_ID, uint max_timestamp){
    logger = log4cplus::Logger::getInstance(LOG4CPLUS_TEXT("LockManager"));
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


void LockManager::index_slot_read_lock(const std::string& hash){
    index_slot_locktable.read_lock(hash);
}

void LockManager::index_slot_write_lock(const std::string& hash){
    index_slot_locktable.write_lock(hash);
}

void LockManager::index_slot_unlock(const std::string& hash){
    index_slot_locktable.unlock(hash);
}

void LockManager::data_slot_read_lock(const std::string& hash){
    data_slot_locktable.read_lock(hash);
}

void LockManager::data_slot_write_lock(const std::string& hash){
    data_slot_locktable.write_lock(hash);
}

void LockManager::data_slot_unlock(const std::string& hash){
    data_slot_locktable.unlock(hash);
}

void LockManager::header_read_lock(const std::string& hash){
    header_locktable.read_lock(hash);
}

void LockManager::header_write_lock(const std::string& hash){
    header_locktable.write_lock(hash);
}

void LockManager::header_unlock(const std::string& hash){
    header_locktable.unlock(hash);
}


bool LockManager::key_write_lock(const std::string& key, uint static_transaction_ID, uint dynamic_transaction_ID, uint* occupier_transaction_ID){
    LOG4CPLUS_DEBUG(logger, LOG4CPLUS_TEXT("key_write_lock, static_transaction_ID: ") << static_transaction_ID 
                        << ", dynamic_transaction_ID: " << dynamic_transaction_ID);
    return key_write_locktable.occupy_lock(key, static_transaction_ID, dynamic_transaction_ID, occupier_transaction_ID);
}

void LockManager::key_write_unlock(const std::string& key, uint static_transaction_ID){
    LOG4CPLUS_DEBUG(logger, LOG4CPLUS_TEXT("key_write_unlock, static_transaction_ID: ") << static_transaction_ID);
    key_write_locktable.occupy_unlock(key, static_transaction_ID);
}

void LockManager::dynamic_transaction_abort_finish(uint dynamic_transaction_ID){
    LOG4CPLUS_DEBUG(logger, LOG4CPLUS_TEXT("dynamic_transaction_abort_finish, dynamic_transaction_ID: ") << dynamic_transaction_ID);
    key_write_locktable.occupy_abort_finish(dynamic_transaction_ID);
}

uint LockManager::get_read_timestamp(std::string* begin_time_nanoseconds){
    uint read_timestamp = 0;
    pthread_rwlock_rdlock(&timestamp_rwlock);
    read_timestamp = max_allocated_timestamp;
    *begin_time_nanoseconds = nanoseconds_since_start();
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

void LockManager::commit_unlock(std::string* commit_time_nanoseconds){
    pthread_rwlock_wrlock(&timestamp_rwlock);
    max_allocated_timestamp += 1;
    *commit_time_nanoseconds = nanoseconds_since_start();
    pthread_rwlock_unlock(&timestamp_rwlock);
    pthread_mutex_unlock(&commit_mutex);
}

uint LockManager::get_dynamic_transaction_ID(){
    uint dynamic_transaction_ID = 0;
    pthread_mutex_lock(&transaction_ID_mutex);
    max_allocated_transaction_ID += 1;
    dynamic_transaction_ID = max_allocated_transaction_ID;
    pthread_mutex_unlock(&transaction_ID_mutex);
    LOG4CPLUS_DEBUG(logger, LOG4CPLUS_TEXT("get_dynamic_transaction_ID: ") << dynamic_transaction_ID);
    return dynamic_transaction_ID;
}

void LockManager::begin_transaction(uint static_transaction_ID){
    LOG4CPLUS_DEBUG(logger, LOG4CPLUS_TEXT("begin_transaction, static_transaction_ID: ") << static_transaction_ID);
    wait_queue_table.insert(static_transaction_ID);
}

void LockManager::end_transaction(uint static_transaction_ID){
    LOG4CPLUS_DEBUG(logger, LOG4CPLUS_TEXT("end_transaction, static_transaction_ID: ") << static_transaction_ID);
    wait_queue_table.release(static_transaction_ID);
}

void LockManager::wait_for_transaction(uint static_transaction_ID){
    LOG4CPLUS_DEBUG(logger, LOG4CPLUS_TEXT("end_transaction, static_transaction_ID: ") << static_transaction_ID);
    wait_queue_table.wait(static_transaction_ID);
}

void LockManager::ban_transactions(){
    /* TODO */
}

void LockManager::allow_transactions(){
    /* TODO */
}