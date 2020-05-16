#include <iostream>
#include <pthread.h>
#include <staydb/util/timer.h>
#include <staydb/pagemanager/pagemanager.h>
#include <staydb/lockmanager/lockmanager.h>
#include <staydb/logmanager/logmanager.h>
#include <staydb/worker/worker.h>
#include <log4cplus/logger.h>
#include <log4cplus/configurator.h>
#include <log4cplus/loggingmacros.h>

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
    log4cplus::initialize();
    log4cplus::PropertyConfigurator::doConfigure("config/log4cplus_configure.ini");
    log4cplus::Logger logger = log4cplus::Logger::getInstance(LOG4CPLUS_TEXT("main"));
    timer_start();
    pthread_mutex_init(&cout_mutex, 0);
    std::cout << "Welcome to StayDB" << std::endl;

    LOG4CPLUS_INFO(logger, LOG4CPLUS_TEXT("create lock manager"));
    LockManager::create_instance(0, 0);
    LOG4CPLUS_INFO(logger, LOG4CPLUS_TEXT("get lock manager instance"));
    LockManager::get_instance();
    LOG4CPLUS_INFO(logger, LOG4CPLUS_TEXT("get page manager"));
    PageManager::get_instance();
    LOG4CPLUS_INFO(logger, LOG4CPLUS_TEXT("get log manager"));
    LogManager::get_instance();

    pthread_t prepare_thread;
    LOG4CPLUS_INFO(logger, LOG4CPLUS_TEXT("create worker for prepare thread"));
    WorkerTask prepare_task = WorkerTask(Worker(0), "inputs/data_prepare.txt", "inputs/data_prepare_out.txt");
    LOG4CPLUS_INFO(logger, LOG4CPLUS_TEXT("creating prepare thread"));
    pthread_create(&prepare_thread, 0, worker_thread, &prepare_task);
    pthread_join(prepare_thread, 0);

    pthread_t task1_thread;
    LOG4CPLUS_INFO(logger, LOG4CPLUS_TEXT("create worker for thread1"));
    WorkerTask task1 = WorkerTask(Worker(1), "inputs/thread_1.txt", "inputs/thread_1_out.txt");
    LOG4CPLUS_INFO(logger, LOG4CPLUS_TEXT("creating thread1"));
    pthread_create(&task1_thread, 0, worker_thread, &task1);
    // pthread_join(task1_thread, 0);

    pthread_t task2_thread;
    LOG4CPLUS_INFO(logger, LOG4CPLUS_TEXT("create worker for thread2"));
    WorkerTask task2 = WorkerTask(Worker(2), "inputs/thread_2.txt", "inputs/thread_2_out.txt");
    LOG4CPLUS_INFO(logger, LOG4CPLUS_TEXT("creating thread2"));
    pthread_create(&task2_thread, 0, worker_thread, &task2);
    // pthread_join(task2_thread, 0);

    pthread_t task3_thread;
    LOG4CPLUS_INFO(logger, LOG4CPLUS_TEXT("create worker for thread3"));
    WorkerTask task3 = WorkerTask(Worker(3), "inputs/thread_3.txt", "inputs/thread_3_out.txt");
    LOG4CPLUS_INFO(logger, LOG4CPLUS_TEXT("creating thread3"));
    pthread_create(&task3_thread, 0, worker_thread, &task3);
    // pthread_join(task3_thread, 0);

    pthread_join(task1_thread, 0);
    pthread_join(task2_thread, 0);
    pthread_join(task3_thread, 0);
    return 0;
}