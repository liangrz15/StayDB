#include <staydb/executor/executor.h>
#include <staydb/util/crc32.h>
#include <staydb/util/md5.h>
#include <cassert>

Executor::Executor(){
    lock_manager = LockManager::get_instance();
    assert(lock_manager != nullptr);
    log_manager = LogManager::get_instance();
    assert(log_manager != nullptr);
}

executor_error_t Executor::read_key(const std::string& key, int* value){
    uint n_index_pages;
    uint n_data_pages;
    std::string hash = calc_hash(key);
    parse_header(hash, &n_index_pages, &n_data_pages);
    uint index_page_ID;
    uint index_slot_ID;
    uint prev_record_page_ID;
    uint prev_record_slot_ID;
    bool index_found = find_index(hash, key, n_index_pages, &index_page_ID, &index_slot_ID, 
                        &prev_record_page_ID, &prev_record_slot_ID);
    if(!index_found){
        return ERROR_READ_NOT_EXIST;
    }
    while(true){
        DataRecord prev_record = get_record(hash, prev_record_page_ID, prev_record_slot_ID);
        if(prev_record.timestamp <= read_timestamp){
            if(prev_record.delete_flag){
                return ERROR_READ_NOT_EXIST;
            }
            *value = prev_record.value;
            return ERROR_NONE;
        }
        if(prev_record.delete_flag){
            return ERROR_READ_NOT_EXIST;
        }
        else{
            prev_record_page_ID = prev_record.prev_page_ID;
            prev_record_slot_ID = prev_record.prev_slot_ID;
        }
    }
    return ERROR_NONE;
}

executor_error_t Executor::write_key(const std::string& key, int value, uint* wait_transaction_ID){
    uint occupier_transaction_ID;
    bool success = lock_manager->key_write_lock(key, static_transaction_ID, &occupier_transaction_ID);
    if(!success){
        *wait_transaction_ID = occupier_transaction_ID;
        return ERROR_WAIT_WRITE;
    }
    uint n_index_pages;
    uint n_data_pages;
    std::string hash = calc_hash(key);
    parse_header(hash, &n_index_pages, &n_data_pages);
    uint index_page_ID;
    uint index_slot_ID;
    uint prev_record_page_ID;
    uint prev_record_slot_ID;
    bool index_found = find_index(hash, key, n_index_pages, &index_page_ID, &index_slot_ID, 
                        &prev_record_page_ID, &prev_record_slot_ID);
    if(!index_found){
        return ERROR_WRITE_NOT_EXIST;
    }
    DataRecord prev_record = get_record(hash, prev_record_page_ID, prev_record_slot_ID);
    if(prev_record.timestamp > read_timestamp){
        return ERROR_RETRY_WRITE;
    }
    DataRecord new_record = DataRecord(false, false, value, prev_record_page_ID, 
                            prev_record_slot_ID, UINT_MAX);
    uint inserted_page_ID;
    uint inserted_slot_ID;
    insert_record(hash, n_data_pages, new_record, &inserted_page_ID, &inserted_slot_ID);
    write_items.push_back(WriteItem(hash, key, inserted_page_ID, inserted_slot_ID, n_index_pages, false, 
                            index_page_ID, index_slot_ID));
    /* update index happen during commit */
    return ERROR_NONE;
}

executor_error_t Executor::insert_key(const std::string& key, int value, uint* wait_transaction_ID){
    uint occupier_transaction_ID;
    bool success = lock_manager->key_write_lock(key, static_transaction_ID, &occupier_transaction_ID);
    if(!success){
        *wait_transaction_ID = occupier_transaction_ID;
        return ERROR_WAIT_WRITE;
    }
    uint n_index_pages;
    uint n_data_pages;
    std::string hash = calc_hash(key);
    parse_header(hash, &n_index_pages, &n_data_pages);
    uint index_page_ID;
    uint index_slot_ID;
    uint prev_record_page_ID;
    uint prev_record_slot_ID;
    bool index_found = find_index(hash, key, n_index_pages, &index_page_ID, &index_slot_ID, 
                        &prev_record_page_ID, &prev_record_slot_ID);
    if(index_found){
        DataRecord prev_record = get_record(hash, prev_record_page_ID, prev_record_slot_ID);
        if(prev_record.timestamp > read_timestamp){
            return ERROR_RETRY_WRITE;
        }
        if(!prev_record.delete_flag){
            return ERROR_INSERT_EXIST;
        }
        DataRecord new_record = DataRecord(true, false, value, prev_record_page_ID, prev_record_slot_ID, UINT_MAX);
        uint inserted_page_ID;
        uint inserted_slot_ID;
        insert_record(hash, n_data_pages, new_record, &inserted_page_ID, &inserted_slot_ID);
        write_items.push_back(WriteItem(hash, key, inserted_page_ID, inserted_slot_ID, n_index_pages, false, 
                            index_page_ID, index_slot_ID));
    }
    else{
        DataRecord new_record = DataRecord(true, false, value, 0, 0, UINT_MAX);
        uint inserted_page_ID;
        uint inserted_slot_ID;
        insert_record(hash, n_data_pages, new_record, &inserted_page_ID, &inserted_slot_ID);
        write_items.push_back(WriteItem(hash, key, inserted_page_ID, inserted_slot_ID, n_index_pages, true));
    }
    
    return ERROR_NONE;
}

executor_error_t Executor::delete_key(const std::string& key, uint* wait_transaction_ID){
    return ERROR_NONE;
}

executor_error_t Executor::abort(){
    log_manager->redo_logs(log_items);
    log_manager->add_log(ABORT_LOG, dynamic_transaction_ID, nullptr, 0, 0, 0, nullptr, nullptr);
    return ERROR_NONE;
}

executor_error_t Executor::commit(){
    write_timestamp = lock_manager->commit_lock_and_get_timestamp();
    for(WriteItem write_item: write_items){
        if(write_item.first_record){
            update_record(write_item.hash, write_item.inserted_page_ID, write_item.inserted_slot_ID, write_timestamp);
            insert_index(write_item.hash, write_item.key, write_item.n_index_pages, write_item.inserted_page_ID, write_item.inserted_slot_ID);
        }
        else{
            update_record(write_item.hash, write_item.inserted_page_ID, write_item.inserted_slot_ID, write_timestamp);
            update_index(write_item.hash, write_item.index_page_ID, write_item.index_slot_ID, write_item.inserted_page_ID,
                            write_item.inserted_slot_ID);
        }
    }
    log_manager->add_log(COMMIT_LOG, dynamic_transaction_ID, nullptr, 0, 0, 0, nullptr, nullptr);
    lock_manager->commit_unlock();
    return ERROR_NONE;
}

void Executor::reset(uint static_transaction_ID){
    this->static_transaction_ID = static_transaction_ID;
    write_items.clear();
    log_items.clear();
    dynamic_transaction_ID = lock_manager->get_dynamic_transaction_ID();
    read_timestamp = lock_manager->get_read_timestamp();
}

std::string Executor::calc_hash(const std::string& key){
    return md5(key);
}

uint Executor::calc_checksum(const unsigned char *buf, int len){
    return xcrc32(buf, len, 0xffffffff);
}

void Executor::parse_header(const std::string& hash, uint* n_index_pages, uint* n_data_pages){

}

bool Executor::find_index(const std::string& hash, const std::string& key, uint n_index_pages, uint* index_page_ID, 
                uint* index_slot_ID, uint* record_page_ID, uint* record_slot_ID){
    return true;
}

DataRecord Executor::get_record(const std::string& hash, uint record_page_ID, uint record_slot_ID){

}

void Executor::insert_record(const std::string& hash, uint n_data_pages, const DataRecord& record, 
                    uint* inserted_page_ID, uint* inserted_slot_ID){

}

/* update_record happen during commit*/
void Executor::update_record(const std::string& hash, uint record_page_ID, uint record_slot_ID, 
                    uint write_timestamp){

}

/* update_index happen during commit*/
void Executor::update_index(const std::string& hash, uint index_page_ID, uint index_slot_ID, 
                    uint record_page_ID, uint record_slot_ID){

}

/* insert_index happen during commit*/
void Executor::insert_index(const std::string& hash, const std::string& key, uint n_index_pages, 
                    uint record_page_ID, uint record_slot_ID){

}