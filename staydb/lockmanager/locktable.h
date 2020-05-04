#pragma once
#include <string>
#include <map>
#include <staydb/lockmanager/bookable_lock.h>

class LockTable{
public:
    LockTable();
    void lock(const std::string& key);
    void unlock(const std::string& key);
    bool occupy_lock(const std::string& key, uint applicant_ID, uint* occupier_ID);
    void occupy_unlock(const std::string& key, uint applicant_ID);

private:
    pthread_rwlock_t rwlock;
    std::map<std::string, BookableLock*> locks;
    BookableLock* find_lock(const std::string& key);
};