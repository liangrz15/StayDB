#include "lockmanager.h"
#include <pthread.h>
#include <cassert>

LockManager::LockManager(){
    pthread_rwlock_init(&index_locks_rwlock, 0);
}

void LockManager::index_lock(const std::string& key){
    BookableLock* lock = nullptr;
    pthread_rwlock_rdlock(&index_locks_rwlock);
    auto it = index_locks.find(key);
    if(it == index_locks.end()){
        pthread_rwlock_unlock(&index_locks_rwlock);
        pthread_rwlock_wrlock(&index_locks_rwlock);
        auto it = index_locks.find(key);
        if(it == index_locks.end()){
            index_locks[key] = new BookableLock();
        }
    }
    lock = index_locks[key];
    lock->book();
    pthread_rwlock_unlock(&index_locks_rwlock);
    
    lock->lock();
}

void LockManager::index_unlock(const std::string& key){
    BookableLock* lock = nullptr;
    pthread_rwlock_rdlock(&index_locks_rwlock);
    BookableLock* lock = index_locks[key];
    pthread_rwlock_unlock(&index_locks_rwlock);
    lock->unlock();
}