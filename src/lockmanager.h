#pragma once
#include "const.h"
#include <string>

/* manager of lock and global transaction information */
class LockManager{

public:
    /* return dynamic_transaction_ID */
    int start_transaction(uint static_transaction_ID);
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
};