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
#include <staydb/util/prepare.h>
#include <staydb/checkpointmanager/checkpoint_manager.h>

pthread_mutex_t cout_mutex;


struct WorkerTask{
    WorkerTask(Worker _worker, std::string _input_file_name, std::string _output_file_name, bool _print_info): worker(_worker),
                input_file_name(_input_file_name), output_file_name(_output_file_name), print_info(_print_info){}
    Worker worker;
    std::string input_file_name;
    std::string output_file_name;
    bool print_info;
};


void* worker_thread(void* task){
    WorkerTask* worker_task = (WorkerTask*)task;
    worker_task->worker.execute_file(worker_task->input_file_name, worker_task->output_file_name);
    if(worker_task->print_info){
        worker_task->worker.print_execute_info();
    }
    delete worker_task;
    return 0;
}


int main(){
    log4cplus::initialize();
    log4cplus::PropertyConfigurator::doConfigure("config/log4cplus_configure.ini");
    log4cplus::Logger logger = log4cplus::Logger::getInstance(LOG4CPLUS_TEXT("main"));
    timer_start();
    pthread_mutex_init(&cout_mutex, 0);
    std::cout << "Welcome to StayDB" << std::endl;

    LOG4CPLUS_INFO(logger, LOG4CPLUS_TEXT("get lock manager instance"));
    LockManager::get_instance();
    LOG4CPLUS_INFO(logger, LOG4CPLUS_TEXT("get page manager"));
    PageManager::get_instance();
    LOG4CPLUS_INFO(logger, LOG4CPLUS_TEXT("get log manager"));
    LogManager::get_instance();
    
    uint max_transaction_ID;
    uint max_timestamp;
    LOG4CPLUS_INFO(logger, LOG4CPLUS_TEXT("start to recoverty"));
    LogManager::get_instance()->recover(&max_transaction_ID, &max_timestamp);
    LockManager::get_instance()->set_max_transaction_ID(max_transaction_ID);
    LockManager::get_instance()->set_max_timestamp(max_timestamp);
    LOG4CPLUS_INFO(logger, LOG4CPLUS_TEXT("recoverty finish"));

    wrap_transaction("inputs/data_prepare.txt", "inputs/data_prepare_wrap.txt");

    pthread_t prepare_thread;
    LOG4CPLUS_INFO(logger, LOG4CPLUS_TEXT("create worker for prepare thread"));
    WorkerTask* prepare_task = new WorkerTask(Worker(0), "inputs/data_prepare_wrap.txt", "inputs/data_prepare_out.txt", false);
    LOG4CPLUS_INFO(logger, LOG4CPLUS_TEXT("creating prepare thread"));
    pthread_create(&prepare_thread, 0, worker_thread, prepare_task);
    pthread_join(prepare_thread, 0);

    CheckpointManager checkpoint_manager;
    checkpoint_manager.start();

    std::vector<pthread_t> task_threads;
    for(int i = 1; i <= 4; i++){
        std::string thread_str = std::string("thread_") + std::to_string(i);
        pthread_t task_thread;
        std::string input_file = std::string("inputs/") + thread_str + std::string(".txt");
        std::cout << input_file << std::endl;
        std::string output_file = std::string("inputs/output_") + thread_str + std::string(".csv");
        LOG4CPLUS_INFO(logger, LOG4CPLUS_TEXT("create worker for thread") << i);
        WorkerTask* task = new WorkerTask(Worker(i), input_file, output_file, true);
        LOG4CPLUS_INFO(logger, LOG4CPLUS_TEXT("creating thread") << i);
        pthread_create(&task_thread, 0, worker_thread, task);
        task_threads.push_back(task_thread);
        //pthread_join(task_thread, 0);
    }
    for(pthread_t task_thread: task_threads){
        pthread_join(task_thread, 0);
    }

    Executor executor;
    std::string begin_time;
    executor.reset(200000000, &begin_time);
    std::vector<std::string> keys;
    keys.push_back("attr_A");
    keys.push_back("attr_B");
    keys.push_back("attr_C");
    keys.push_back("attr_D");
    for(std::string key: keys){
        int value;
        assert(executor.read_key(key, &value) == ERROR_NONE);
        std::cout << key << ": " << value << std::endl;
    }

    checkpoint_manager.end();
    std::cout << "execute total time: " << nanoseconds_since_start() << std::endl;
    return 0;
}