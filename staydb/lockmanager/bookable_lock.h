#pragma once
#include <staydb/const.h>
#include <pthread.h>

class BookableLock{
public:
    void book();
    void read_lock();
    void write_lock();
    void unlock();
    bool occupy_lock(uint applicant_ID, uint* occupier_ID);
    void occupy_unlock(uint applicant_ID);
    bool safe_to_delete();
    BookableLock();
    ~BookableLock();

private:
    int n_booked;
    int n_locked;
    bool occupied;
    uint occupier_ID;
    pthread_mutex_t book_mutex;
    pthread_rwlock_t rwlock;
};