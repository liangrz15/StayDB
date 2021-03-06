#pragma once
#include <staydb/const.h>
#include <string>
#include <map>
#include <staydb/lockmanager/bookable_lock.h>
#include <pthread.h>
#include <staydb/lockmanager/locktable.h>
#include <staydb/lockmanager/waitqueue_table.h>
#include <staydb/util/timer.h>
#include <staydb/lockmanager/occupy_locktable.h>
#include <log4cplus/loggingmacros.h>
#include <log4cplus/logger.h>

/* manager of lock and global transaction information */
class LockManager{
public:
    static LockManager* get_instance();

    /* The static transaction ID is generated by the input file */
    /* It is used by the worker itself as well as the write lock */
    /* The dynamic transaction ID is generated by the LockManager */
    /* It is used for logging and recovery */
    /* As a transaction may abort and restart */
    uint get_dynamic_transaction_ID();
    void begin_transaction(uint static_transaction_ID);
    void end_transaction(uint static_transaction_ID);
    void dynamic_transaction_abort_finish(uint dynamic_transaction_ID);
    void wait_for_transaction(uint static_transaction_ID);

    bool key_write_lock(const std::string& key, uint static_transaction_ID, uint dynamic_transaction_ID, uint* occupier_transaction_ID);
    void key_write_unlock(const std::string& key, uint static_transaction_ID);
    void index_slot_read_lock(const std::string& hash);
    void index_slot_write_lock(const std::string& hash);
    void index_slot_unlock(const std::string& hash);
    void data_slot_read_lock(const std::string& hash);
    void data_slot_write_lock(const std::string& hash);
    void data_slot_unlock(const std::string& hash);
    void header_read_lock(const std::string& hash);
    void header_write_lock(const std::string& hash);
    void header_unlock(const std::string& hash);

    void ban_transactions();
    void allow_transactions();

    uint get_read_timestamp(std::string* begin_time_nanoseconds);
    uint commit_lock_and_get_timestamp();
    void commit_unlock(std::string* commit_time_nanoseconds);

    uint get_max_allocated_transaction_ID(){
        return max_allocated_transaction_ID;
    }
    uint get_max_allocated_timestamp(){
        pthread_rwlock_rdlock(&timestamp_rwlock);
        uint _max_allocated_timestamp = max_allocated_timestamp;
        pthread_rwlock_unlock(&timestamp_rwlock);
        return _max_allocated_timestamp;
    }
    void set_max_transaction_ID(uint max_transaction_ID);
    void set_max_timestamp(uint max_timestamp);

private:
    log4cplus::Logger logger;
    static LockManager* instance;
    LockManager();
    LockTable index_slot_locktable;
    LockTable data_slot_locktable;
    LockTable header_locktable;
    OccupyLockTable key_write_locktable;
    WaitQueueTable wait_queue_table;
    uint max_allocated_timestamp;
    uint max_allocated_transaction_ID;
    pthread_rwlock_t timestamp_rwlock;
    pthread_mutex_t transaction_ID_mutex;
    pthread_mutex_t commit_mutex;

    /* begin of variables for checkpoint */
    pthread_mutex_t checkpoint_mutex;
    pthread_cond_t worker_execute_cond;
    pthread_cond_t checkpoint_manager_cond;
    int n_executing_threads;
    bool checkpoint_ready;
    /* end of variables for checkpoint   */
};