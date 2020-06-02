#include <staydb/checkpointmanager/checkpoint_manager.h>
#include <unistd.h>


CheckpointManager::CheckpointManager(){
    task_finish = false;
    pthread_mutex_init(&join_mutex, 0);
}

void CheckpointManager::start(){
    pthread_create(&working_thread, 0, run_in_another_thread, this);
}

void CheckpointManager::end(){
    pthread_mutex_lock(&join_mutex);
    task_finish = true;
    pthread_mutex_unlock(&join_mutex);
    pthread_join(working_thread, 0);
}


void* CheckpointManager::run_in_another_thread(void* manager){
    CheckpointManager* checkpoint_manager = (CheckpointManager*)manager;
    checkpoint_manager->run();
    return 0;
}

void CheckpointManager::run(){
    while(true){
        sleep(sleep_second);
        pthread_mutex_lock(&join_mutex);
        bool will_finish = task_finish;
        pthread_mutex_unlock(&join_mutex);
        make_checkpoint();
        if(will_finish){
            break;
        }
    }
}

void CheckpointManager::make_checkpoint(){
    LockManager::get_instance()->ban_transactions();
    PageManager::get_instance()->flush_all_pages();
    LogManager::get_instance()->add_checkpoint_log();
    LogManager::get_instance()->flush_logs();
    LockManager::get_instance()->allow_transactions();
}