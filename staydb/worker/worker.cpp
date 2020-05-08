#include <staydb/worker/worker.h>
#include <cassert>
#include <vector>
#include <iostream>
#include <staydb/util/timer.h>

extern pthread_mutex_t cout_mutex;

Worker::Worker(uint _worker_ID): worker_ID(_worker_ID), transaction_active(false){
    lock_manager = LockManager::get_instance();
    assert(lock_manager != nullptr);
}

void Worker::execute_file(std::string input_file_name, std::string output_file_name){
    std::ifstream in_stream(input_file_name);
    std::ofstream out_stream(output_file_name);
    if(!in_stream){
        assert(0);
    }

    std::string command_type;
    std::string key;
    uint transaction_ID;
    int value;
    std::string first_operand;
    std::string op;
    int second_operand;
    std::vector<Command> commands;
    while(true){
        in_stream >> command_type;
        if(in_stream.fail()){
            break;
        }
        if(command_type == "BEGIN"){
            in_stream >> transaction_ID;
            commands.push_back(Command(BEGIN, "", transaction_ID));
        }
        else if(command_type == "INSERT"){
            in_stream >> key >> value;
            commands.push_back(Command(INSERT, key, 0, value));
        }
        else if(command_type == "READ"){
            in_stream >> key;
            commands.push_back(Command(READ, key));
        }
        else if(command_type == "SET"){
            in_stream >> key >> first_operand >> op >> second_operand;
            key = key.substr(0, key.length() - 1); // get rid of the ","
            commands.push_back(Command(SET, key, 0, 0, first_operand, op, second_operand));
        }
        else if(command_type == "COMMIT"){
            in_stream >> transaction_ID;
            commands.push_back(Command(COMMIT, "", transaction_ID));
            execute_commands(commands);
            out_stream << transaction_output;
            commands.clear();
        }
    }
}

void Worker::execute_commands(const std::vector<Command>& commands){
    while(true){
        int n_commands = commands.size();
        ExecuteStatus status;
        transaction_output.clear();
        bool finish = false;
        for(int i = 0; i < n_commands; i++){
            const Command& command = commands[i];
            if(command.command_type == BEGIN){
                status = execute_begin(command.transaction_ID);
            }
            else if(command.command_type == INSERT){
                status = execute_insert(command.key, command.value);
            }
            else if(command.command_type == READ){
                status = execute_read(command.key);
            }
            else if(command.command_type == SET){
                status = execute_set(command.key, command.first_operand, command.op, command.second_operand);
            }
            else if(command.command_type == COMMIT){
                status = execute_commit(command.transaction_ID);
            }
            if(status == EXECUTE_NORMAL){
            }
            else if(status == EXECUTE_ABORT_REDO){
                finish = false;
                break;
            }
            else if(status == EXECUTE_ABORT_FINISH){
                transaction_output.clear();
                finish = true;
                break;
            }
            else if(status == EXECUTE_COMMIT){
                finish = true;
                break;
            }
            else{
                assert(0);
            }
        }
        if(finish){
            transaction_active = false;
            lock_manager->end_transaction(transaction_ID);
            break;
        }
    }
}

ExecuteStatus Worker::execute_begin(uint transaction_ID){
    assert(transaction_output == "");
    if(transaction_active == false){
        transaction_active = true;
        this->transaction_ID = transaction_ID;
        lock_manager->begin_transaction(transaction_ID);
    }
    else{
        assert(transaction_ID == this->transaction_ID);
    }
    local_map.clear();
    executor.reset(transaction_ID);
    append_output_row("BEGIN");
    return EXECUTE_NORMAL;
}

ExecuteStatus Worker::execute_insert(const std::string& key, int value){
    assert(transaction_active);
    uint wait_transaction_ID;
    executor_error_t err = executor.insert_key(key, value, &wait_transaction_ID);
    if(err == ERROR_WAIT_WRITE){
        executor.abort();
        lock_manager->wait_for_transaction(wait_transaction_ID);
        return EXECUTE_ABORT_REDO;
    }
    else if(err == ERROR_INSERT_EXIST){
        executor.abort();
        pthread_mutex_lock(&cout_mutex);
        std::cout << "thread " << worker_ID << ": " << std::endl;
        std::cout << "insert a key that exists: " << key << std::endl;
        std::cout << "aborted" << std::endl;
        pthread_mutex_unlock(&cout_mutex);
        return EXECUTE_ABORT_FINISH;
    }
    else{
        assert(err == ERROR_NONE);
        assert(local_map.find(key) == local_map.end());
        local_map[key] = value;
        return EXECUTE_NORMAL;
    }
}

executor_error_t Worker::get_value_for_key(const std::string& key, int* value){
    if(local_map.find(key) != local_map.end()){
        *value = local_map[key];
        return ERROR_NONE;
    }
    else{
        executor_error_t err = executor.read_key(key, value);
        if(err == ERROR_NONE){
            local_map[key] = *value;
        }
        return err;
    }
}

ExecuteStatus Worker::execute_read(const std::string& key){
    assert(transaction_active);
    int value;
    executor_error_t err = get_value_for_key(key, &value);
    if(err == ERROR_READ_NOT_EXIST){
        pthread_mutex_lock(&cout_mutex);
        std::cout << "thread " << worker_ID << ": " << std::endl;
        std::cout << "read a key that does not exist: " << key << std::endl;
        std::cout << "aborted" << std::endl;
        pthread_mutex_unlock(&cout_mutex);
        executor.abort();
        return EXECUTE_ABORT_FINISH;
    }
    assert(err = ERROR_NONE);
    append_output_row(key, value);
    return EXECUTE_NORMAL;    
}


ExecuteStatus Worker::execute_set(const std::string& key, const std::string& first_operand, const std::string& op, int second_operand){
    assert(transaction_active);
    int value;
    executor_error_t err = get_value_for_key(first_operand, &value);
    if(err == ERROR_READ_NOT_EXIST){
        executor.abort();
        pthread_mutex_lock(&cout_mutex);
        std::cout << "thread " << worker_ID << ": " << std::endl;
        std::cout << "read a key that does not exist: " << key << std::endl;
        std::cout << "aborted" << std::endl;
        pthread_mutex_unlock(&cout_mutex);
        return EXECUTE_ABORT_FINISH;
    }
    assert(err = ERROR_NONE);
    append_output_row(first_operand, value);

    if(op == "+"){
        value += second_operand;
    }
    else if(op == "-"){
        value -= second_operand;
    }
    else{
        assert(0);
    }

    uint wait_transaction_ID;
    err = executor.write_key(key, value, &wait_transaction_ID);
    if(err == ERROR_WAIT_WRITE){
        executor.abort();
        lock_manager->wait_for_transaction(wait_transaction_ID);
        return EXECUTE_ABORT_REDO;
    }
    else if(err == ERROR_RETRY_WRITE){
        executor.abort();
        return EXECUTE_ABORT_REDO;
    }
    else if(err == ERROR_WRITE_NOT_EXIST){
        executor.abort();
        pthread_mutex_lock(&cout_mutex);
        std::cout << "thread " << worker_ID << ": " << std::endl;
        std::cout << "update a key that does not exist: " << key << std::endl;
        std::cout << "aborted" << std::endl;
        pthread_mutex_unlock(&cout_mutex);
        return EXECUTE_ABORT_FINISH;
    }
    assert(err == ERROR_NONE);
    return EXECUTE_NORMAL;
}

ExecuteStatus Worker::execute_commit(uint transaction_ID){
    assert(transaction_active);
    executor_error_t err = executor.commit();
    assert(err == ERROR_NONE);
    return EXECUTE_COMMIT;
}

void Worker::append_output_row(std::string type){
    std::string time = nanoseconds_since_start();
    std::string row = std::to_string(transaction_ID) + "," + type + "," + time + "," + "\n";
    transaction_output += row;
}

void Worker::append_output_row(std::string type, int value){
    std::string time = nanoseconds_since_start();
    std::string row = std::to_string(transaction_ID) + "," + type + "," + time + "," + std::to_string(value) + "\n";
    transaction_output += row;
}