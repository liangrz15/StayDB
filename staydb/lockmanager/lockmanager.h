#pragma once
#include <staydb/const.h>
#include <string>
#include <map>
#include <staydb/lockmanager/bookable_lock.h>
#include <pthread.h>
#include <staydb/lockmanager/locktable.h>
#include <staydb/lockmanager/waitqueue_table.h>

/* manager of lock and global transaction information */
class LockManager{
public:
    static LockManager* get_instance();
    static void create_instance(uint max_transaction_ID, uint max_timestamp);

    uint get_dynamic_transaction_ID();
    void begin_transaction(uint static_transaction_ID);
    void end_transaction(uint static_transaction_ID);
    void wait_for_transaction(uint static_transaction_ID);

    bool key_write_lock(const std::string& key, uint transaction_ID, uint* occupier_transaction_ID);
    void key_write_unlock(const std::string& key, uint static_transaction_ID);
    void index_lock(const std::string& key);
    void index_unlock(const std::string& key);
    void slot_lock(const std::string& hash);
    void slot_unlock(const std::string& hash);

    void ban_transactions();
    void allow_transactions();

    uint get_read_timestamp();
    uint commit_lock_and_get_timestamp();
    void commit_unlock();

private:
    static LockManager* instance;
    LockManager(uint max_transaction_ID, uint max_timestamp);
    LockTable index_locktable;
    LockTable slot_locktable;
    LockTable key_write_locktable;
    WaitQueueTable wait_queue_table;
    uint max_allocated_timestamp;
    uint max_allocated_transaction_ID;
    pthread_rwlock_t timestamp_rwlock;
    pthread_mutex_t transaction_ID_mutex;
    pthread_mutex_t commit_mutex;

};