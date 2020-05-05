#pragma once
#include <string>
#include <staydb/const.h>

enum executor_error_t{
    ERROR_NONE,
    ERROR_WAIT_WRITE,
    ERROR_INSERT_EXIST,
    ERROR_READ_NOT_EXIST
};


/* executor for read/write/insert/delete command */
class Executor{

public:
    executor_error_t read_key(const std::string& key, int* value);
    executor_error_t write_key(const std::string& key, int value, uint* wait_transaction_ID);
    executor_error_t insert_key(const std::string& key, int value, uint* wait_transaction_ID);
    executor_error_t delete_key(const std::string& key, uint* wait_transaction_ID);
    executor_error_t abort();
    executor_error_t commit();
    void reset(uint static_transaction_ID);
};