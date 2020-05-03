#pragma once
#include "const.h"
#include <string>
#include <map>
#include "bookable_lock.h"
#include <pthread.h>

/* manager of lock and global transaction information */
class LockManager{
public:
    LockManager* get_instance();
    /* return dynamic_transaction_ID */
    int start_transaction();
    void end_transaction(uint dynamic_transaction_ID);
    void wait_for_transaction(uint dynamic_transaction_ID);

    int key_write_lock(const std::string& key);
    void key_write_unlock(const std::string& key);
    void index_lock(const std::string& key);
    void index_unlock(const std::string& key);
    void slot_lock(const std::string& hash);
    void slot_unlock(const std::string& hash);

    void ban_transactions();
    void allow_transactions();

private:
    LockManager* instance;
    LockManager();
    std::map<std::string, BookableLock*> index_locks;
    std::map<std::string, BookableLock*> slot_locks;
    pthread_rwlock_t index_locks_rwlock;
};