#include <iostream>
#include <pthread.h>
#include <staydb/util/timer.h>
#include <staydb/pagemanager/pagemanager.h>
#include <staydb/lockmanager/lockmanager.h>
#include <staydb/logmanager/logmanager.h>
#include <staydb/worker/worker.h>

pthread_mutex_t cout_mutex;

struct WorkerTask{
    WorkerTask(Worker _worker, std::string _input_file_name, std::string _output_file_name): worker(_worker),
                input_file_name(_input_file_name), output_file_name(_output_file_name){}
    Worker worker;
    std::string input_file_name;
    std::string output_file_name;
};


void* worker_thread(void* task){
    WorkerTask* worker_task = (WorkerTask*)task;
    worker_task->worker.execute_file(worker_task->input_file_name, worker_task->output_file_name);
    return 0;
}


int main(){
    timer_start();
    pthread_mutex_init(&cout_mutex, 0);
    std::cout << "Welcome to StayDB" << std::endl;

    LockManager::create_instance(0, 0);
    LockManager* lock_manager = LockManager::get_instance();
    PageManager* page_manager = PageManager::get_instance();
    LogManager* log_manager = LogManager::get_instance();

    pthread_t prepare_thread;
    WorkerTask prepare_task = WorkerTask(Worker(0), "../inputs/data_prepare", "../inputs/data_prepare_out.txt");
    pthread_create(&prepare_thread, 0, worker_thread, &prepare_task);
    pthread_join(prepare_thread, 0);

    pthread_t task1_thread;
    WorkerTask task1 = WorkerTask(Worker(1), "../inputs/thread_1", "../inputs/thread_1_out.txt");
    pthread_create(&task1_thread, 0, worker_thread, &task1_thread);
    pthread_join(task1_thread, 0);
    return 0;
}