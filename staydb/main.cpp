#include <iostream>
#include <pthread.h>
#include <staydb/util/timer.h>

pthread_mutex_t cout_mutex;


int main(){
    timer_start();
    pthread_mutex_init(&cout_mutex, 0);
    std::cout << "Welcome to StayDB" << std::endl;
    return 0;
}