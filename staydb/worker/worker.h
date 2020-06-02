#pragma once

#include <string>
#include <fstream>
#include <staydb/const.h>
#include <vector>
#include <staydb/executor/executor.h>
#include <staydb/lockmanager/lockmanager.h>
#include <map>
#include <log4cplus/loggingmacros.h>
#include <log4cplus/logger.h>


enum CommandType{
    BEGIN,
    INSERT,
    READ,
    SET,
    COMMIT
};

enum ExecuteStatus{
    EXECUTE_NORMAL,
    EXECUTE_COMMIT,
    EXECUTE_ABORT_REDO,
    EXECUTE_ABORT_FINISH
};

struct Command{
    Command(CommandType _command_type, std::string _key, uint _transaction_ID = 0, int _value = 0, std::string _first_operand = "",
            std::string _op = "", int _second_operand = 0) : command_type(_command_type), key(_key), 
            transaction_ID(_transaction_ID), value(_value), first_operand(_first_operand), op(_op), second_operand(_second_operand) {}
    CommandType command_type;
    std::string key;
    uint transaction_ID;
    int value;
    std::string first_operand;
    std::string op;
    int second_operand;
};


/* The thread worker that parses the command and manage the thread transaction. */
class Worker{

public:
    void execute_file(std::string input_file_name, std::string output_file_name);
    Worker(uint worker_ID);
    void print_execute_info();

private:
    log4cplus::Logger logger;
    void execute_commands(const std::vector<Command>& commands);
    uint worker_ID;
    uint transaction_ID;
    ExecuteStatus execute_begin(uint transaction_ID);
    ExecuteStatus execute_insert(const std::string& key, int value);
    ExecuteStatus execute_read(const std::string& key);
    ExecuteStatus execute_set(const std::string& key, const std::string& first_operand, const std::string& op, int second_operand);
    ExecuteStatus execute_commit(uint transaction_ID);
    std::string transaction_output;
    Executor executor;
    LockManager* lock_manager;
    std::map<std::string, int> local_map;
    void append_output_row(std::string type);
    void append_output_row(std::string type, int value);
    bool transaction_active;
    executor_error_t get_value_for_key(const std::string& key, int* value);
    void append_output_row_with_time(std::string type, const std::string& time_nanosecond);
    bool first_transaction_start;
    std::string first_begin_time;
    std::string last_end_time;

};