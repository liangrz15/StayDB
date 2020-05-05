#include <staydb/executor/executor.h>

executor_error_t Executor::read_key(const std::string& key, int* value){
    return ERROR_NONE;
}

executor_error_t Executor::write_key(const std::string& key, int value, uint* wait_transaction_ID){
    return ERROR_NONE;
}

executor_error_t Executor::insert_key(const std::string& key, int value, uint* wait_transaction_ID){
    return ERROR_NONE;
}

executor_error_t Executor::delete_key(const std::string& key, uint* wait_transaction_ID){
    return ERROR_NONE;
}

executor_error_t Executor::abort(){
    return ERROR_NONE;
}

executor_error_t Executor::commit(){
    return ERROR_NONE;
}

void Executor::reset(uint static_transaction_ID){
    
}