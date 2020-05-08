#include <staydb/lockmanager/bookable_lock.h>
#include <cassert>

BookableLock::BookableLock(){
    n_booked = 0;
    n_locked = 0;
    occupied = false;
    occupier_ID = 0;
    pthread_mutex_init(&book_mutex, 0);
    pthread_rwlock_init(&rwlock, 0);
}

BookableLock::~BookableLock(){

}

void BookableLock::book(){
    pthread_mutex_lock(&book_mutex);
    n_booked += 1;
    pthread_mutex_unlock(&book_mutex);
}

void BookableLock::read_lock(){
    pthread_mutex_lock(&book_mutex);
    n_booked -= 1;
    n_locked += 1;
    pthread_mutex_unlock(&book_mutex);

    pthread_rwlock_rdlock(&rwlock);
}

void BookableLock::write_lock(){
    pthread_mutex_lock(&book_mutex);
    n_booked -= 1;
    n_locked += 1;
    pthread_mutex_unlock(&book_mutex);

    pthread_rwlock_wrlock(&rwlock);
}

void BookableLock::unlock(){
    pthread_mutex_lock(&book_mutex);
    n_locked -= 1;
    pthread_mutex_unlock(&book_mutex);

    pthread_rwlock_unlock(&rwlock);
}

bool BookableLock::occupy_lock(uint applicant_ID, uint* occupier_ID){
    bool ok;
    pthread_mutex_lock(&book_mutex);
    n_booked -= 1;
    if(occupied){
        ok = false;
        *occupier_ID = this->occupier_ID;
    }
    else{
        occupied = true;
        ok = true;
        this->occupier_ID = applicant_ID;
    }
    pthread_mutex_unlock(&book_mutex);
    return ok;
}

void BookableLock::occupy_unlock(uint applicant_ID){
    pthread_mutex_lock(&book_mutex);
    assert(occupied);
    assert(occupier_ID == applicant_ID);
    occupied = false;
    pthread_mutex_unlock(&book_mutex);
}

bool BookableLock::safe_to_delete(){
    pthread_mutex_lock(&book_mutex);
    bool safe = (n_locked == 0) && (n_booked == 0) && (occupied == false);
    pthread_mutex_unlock(&book_mutex);
    return safe;
}