#include "locktable.h"

LockTable::LockTable(){
    pthread_rwlock_init(&rwlock, 0);
}

BookableLock* LockTable::find_lock(const std::string& key){
    BookableLock* lock = nullptr;
    pthread_rwlock_rdlock(&rwlock);
    auto it = locks.find(key);
    if(it == locks.end()){
        pthread_rwlock_unlock(&rwlock);
        pthread_rwlock_wrlock(&rwlock);
        auto it = locks.find(key);
        if(it == locks.end()){
            locks[key] = new BookableLock();
        }
    }
    lock = locks[key];
    lock->book();
    pthread_rwlock_unlock(&rwlock);
    return lock;
}

void LockTable::lock(const std::string& key){
    BookableLock* lock = find_lock(key);
    lock->lock();
}

void LockTable::unlock(const std::string& key){
    BookableLock* lock = nullptr;
    pthread_rwlock_rdlock(&rwlock);
    BookableLock* lock = locks[key];
    pthread_rwlock_unlock(&rwlock);
    lock->unlock();
}

bool LockTable::occupy_lock(const std::string& key, uint applicant_ID, uint* occupier_ID){
    BookableLock* lock = find_lock(key);
    bool lock_success = lock->occupy_lock(applicant_ID, occupier_ID);
    return lock_success;
}