#pragma once
#include <pthread.h>
#include <staydb/lockmanager/lockmanager.h>
#include <staydb/pagemanager/pagemanager.h>
#include <staydb/logmanager/logmanager.h>

class CheckpointManager{
public:
    void start();
    void end();
    CheckpointManager();

private:
    const int sleep_second = 1;
    pthread_t working_thread;
    pthread_mutex_t join_mutex;
    bool task_finish;
    static void* run_in_another_thread(void* manager);
    void run();
    void make_checkpoint();
};