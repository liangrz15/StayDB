#include <staydb/executor/executor.h>
#include <staydb/util/crc32.h>
#include <staydb/util/md5.h>
#include <staydb/util/timer.h>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <algorithm>

Executor::Executor(){
    logger = log4cplus::Logger::getInstance(LOG4CPLUS_TEXT("executor"));
    lock_manager = LockManager::get_instance();
    assert(lock_manager != nullptr);
    log_manager = LogManager::get_instance();
    assert(log_manager != nullptr);
    page_manager = PageManager::get_instance();
    assert(page_manager != nullptr);
}

executor_error_t Executor::read_key(const std::string& key, int* value){
    uint n_index_pages;
    uint n_data_pages;
    std::string hash = calc_hash(key);
    parse_header(hash, &n_index_pages, &n_data_pages);
    LOG4CPLUS_DEBUG(logger, LOG4CPLUS_TEXT("n_index_pages: ") << n_index_pages);
    LOG4CPLUS_DEBUG(logger, LOG4CPLUS_TEXT("n_data_pages: ") << n_data_pages);
    uint index_page_ID;
    uint index_slot_ID;
    uint prev_record_page_ID;
    uint prev_record_slot_ID;
    bool index_found = find_index(hash, key, n_index_pages, &index_page_ID, &index_slot_ID, 
                        &prev_record_page_ID, &prev_record_slot_ID);
    if(!index_found){
        LOG4CPLUS_WARN(logger, LOG4CPLUS_TEXT("index not found"));
        return ERROR_READ_NOT_EXIST;
    }
    while(true){
        DataRecord prev_record = get_record(hash, prev_record_page_ID, prev_record_slot_ID);
        if(prev_record.timestamp <= read_timestamp){
            if(prev_record.delete_flag){
                return ERROR_READ_NOT_EXIST;
            }
            LOG4CPLUS_WARN(logger, LOG4CPLUS_TEXT("prev_record") << prev_record.to_str());
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
    std::string hash = calc_hash(key);
    uint occupier_transaction_ID;
    if(std::find(locked_write_keys.begin(), locked_write_keys.end(), key) == locked_write_keys.end()){
        bool success = lock_manager->key_write_lock(key, static_transaction_ID, dynamic_transaction_ID, &occupier_transaction_ID);
        if(!success){
            *wait_transaction_ID = occupier_transaction_ID;
            abort_due_to_fail_write_lock = true;
            return ERROR_WAIT_WRITE;
        }
        locked_write_keys.push_back(key);
    }
    else{
        for(WriteItem write_item: write_items){
            if(write_item.key == key){
                uint inserted_page_ID = write_item.inserted_page_ID;
                uint inserted_slot_ID = write_item.inserted_slot_ID;
                update_record(hash, inserted_page_ID, inserted_slot_ID, RECORD_VALUE, value);
                return ERROR_NONE;
            }
        }
        assert(0);
    }
    
    uint n_index_pages;
    uint n_data_pages;
    
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
    if(std::find(locked_write_keys.begin(), locked_write_keys.end(), key) == locked_write_keys.end()){
        bool success = lock_manager->key_write_lock(key, static_transaction_ID, dynamic_transaction_ID, &occupier_transaction_ID);
        if(!success){
            *wait_transaction_ID = occupier_transaction_ID;
            abort_due_to_fail_write_lock = true;
            return ERROR_WAIT_WRITE;
        }
        locked_write_keys.push_back(key);
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
        LOG4CPLUS_DEBUG(logger, LOG4CPLUS_TEXT("insert record found"));
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
        LOG4CPLUS_DEBUG(logger, LOG4CPLUS_TEXT("insert record not found"));
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
    log_manager->undo_logs(log_items);
    log_manager->add_log(ABORT_LOG, dynamic_transaction_ID, 0, 0, 0, nullptr, nullptr, nullptr);
    for(std::string key: locked_write_keys){
        lock_manager->key_write_unlock(key, static_transaction_ID);
    }
    if(abort_due_to_fail_write_lock){
        lock_manager->dynamic_transaction_abort_finish(dynamic_transaction_ID);
    }
    
    return ERROR_NONE;
}

executor_error_t Executor::commit(std::string* commit_time_nanoseconds){
    LOG4CPLUS_INFO(logger, LOG4CPLUS_TEXT("transaction ") << static_transaction_ID << " start to commit");
    write_timestamp = lock_manager->commit_lock_and_get_timestamp();
    for(WriteItem write_item: write_items){
        LOG4CPLUS_INFO(logger, LOG4CPLUS_TEXT("write_item: ") << write_item.to_str());
        if(write_item.first_record){
            update_record(write_item.hash, write_item.inserted_page_ID, write_item.inserted_slot_ID, RECORD_TIMESTAMP, write_timestamp);
            insert_index(write_item.hash, write_item.key, write_item.n_index_pages, write_item.inserted_page_ID, write_item.inserted_slot_ID);
        }
        else{
            update_record(write_item.hash, write_item.inserted_page_ID, write_item.inserted_slot_ID, RECORD_TIMESTAMP, write_timestamp);
            update_index(write_item.hash, write_item.index_page_ID, write_item.index_slot_ID, write_item.inserted_page_ID,
                            write_item.inserted_slot_ID);
        }
    }
    log_manager->add_log(COMMIT_LOG, dynamic_transaction_ID, 0, 0, 0, nullptr, nullptr, nullptr);
    if(!write_items.empty()){
        log_manager->flush_logs();
    }
    for(std::string key: locked_write_keys){
        lock_manager->key_write_unlock(key, static_transaction_ID);
    }
    lock_manager->commit_unlock(commit_time_nanoseconds);
    return ERROR_NONE;
}

void Executor::reset(uint static_transaction_ID, std::string* begin_time_nanoseconds){
    this->static_transaction_ID = static_transaction_ID;
    write_items.clear();
    log_items.clear();
    locked_write_keys.clear();
    dynamic_transaction_ID = lock_manager->get_dynamic_transaction_ID();
    read_timestamp = lock_manager->get_read_timestamp(begin_time_nanoseconds);
    abort_due_to_fail_write_lock = false;
}

std::string Executor::calc_hash(const std::string& key){
    return md5(key).substr(0, HASH_LENGTH);
}

uint Executor::calc_checksum(const unsigned char *buf, int len){
    return xcrc32(buf, len, 0xffffffff);
}

void Executor::parse_header(const std::string& hash, uint* n_index_pages, uint* n_data_pages){
    lock_manager->header_read_lock(hash);
    HeaderFilePage* header_page = nullptr;
    int header_pool_ID = page_manager->get_page(hash, HEADER_TYPE, 0, (void**)&header_page);
    bool header_valid = check_header_valid(header_page);
    if(!header_valid){
        lock_manager->header_unlock(hash);
        lock_manager->header_write_lock(hash);
        header_valid = check_header_valid(header_page);
        if(!header_valid){
            HeaderFilePage* backup_page = nullptr;
            int backup_pool_ID = page_manager->get_page(hash, BACKUP_TYPE, 0, (void**)&backup_page);
            bool backup_valid = check_header_valid(backup_page);
            if(backup_valid){
                memcpy(header_page, backup_page, PAGE_SIZE);
                page_manager->flush_page(header_pool_ID);
                page_manager->release_page(backup_pool_ID);
            }
            else{
                backup_page->n_data_pages = 0;
                backup_page->n_index_pages = 0;
                fill_header_checksum(backup_page);
                page_manager->flush_page(backup_pool_ID);
                page_manager->release_page(backup_pool_ID);
                header_page->n_data_pages = 0;
                header_page->n_index_pages = 0;
                fill_header_checksum(header_page);
                page_manager->flush_page(header_pool_ID);
            }
        }
    }
    *n_index_pages = header_page->n_index_pages;
    *n_data_pages = header_page->n_data_pages;
    page_manager->release_page(header_pool_ID);
    lock_manager->header_unlock(hash);    
}

bool Executor::find_index(const std::string& hash, const std::string& key, uint n_index_pages, uint* index_page_ID, 
                uint* index_slot_ID, uint* record_page_ID, uint* record_slot_ID){
    if(n_index_pages == 0){
        return false;
    }
    bool found = false;
    lock_manager->index_slot_read_lock(hash);
    for(uint i = 0; i < n_index_pages; i++){
        IndexFilePage* index_page = nullptr;
        int index_page_pool_ID = page_manager->get_page(hash, INDEX_TYPE, i, (void**)&index_page);
        for(uint slot_ID = 0; slot_ID < N_INDEX_ITEMS_PER_PAGE; slot_ID++){
            bool occupy = get_index_slot_bit(index_page, slot_ID);
            if(occupy){
                const IndexItem& index_item = index_page->index_items[slot_ID];
                if(index_item.key == key){
                    *index_page_ID = i;
                    *index_slot_ID = slot_ID;
                    *record_page_ID = index_item.last_page_ID;
                    *record_slot_ID = index_item.last_slot_ID;
                    found = true;
                    break;
                }
            }
        }
        page_manager->release_page(index_page_pool_ID);
        if(found){
            break;
        }
    }
    lock_manager->index_slot_unlock(hash);
    return found;
}

DataRecord Executor::get_record(const std::string& hash, uint record_page_ID, uint record_slot_ID){
    DataFilePage* data_page = nullptr;
    int data_page_pool_ID = page_manager->get_page(hash, DATA_TYPE, record_page_ID, (void**)&data_page);
    DataRecord record = data_page->data_records[record_slot_ID];
    page_manager->release_page(data_page_pool_ID);
    return record;
}

void Executor::insert_record(const std::string& hash, uint n_data_pages, const DataRecord& record, 
                    uint* inserted_page_ID, uint* inserted_slot_ID){
    LOG4CPLUS_DEBUG(logger, LOG4CPLUS_TEXT("insert record: ") << record.to_str());
    lock_manager->data_slot_write_lock(hash);
    if(n_data_pages > 0){
        LOG4CPLUS_DEBUG(logger, LOG4CPLUS_TEXT("n_data_pages > 0"));
        uint page_ID = n_data_pages - 1;
        if(try_insert_record_to_page(hash, page_ID, record, inserted_slot_ID)){
            *inserted_page_ID = page_ID;
            lock_manager->data_slot_unlock(hash);
            return;
        }
    }
    else{
        LOG4CPLUS_DEBUG(logger, LOG4CPLUS_TEXT("n_data_pages == 0"));
    }
    lock_manager->header_write_lock(hash);
    HeaderFilePage* header_page = nullptr;
    int header_page_pool_ID = page_manager->get_page(hash, HEADER_TYPE, 0, (void**)&header_page);
    if(header_page->n_data_pages > n_data_pages){
        LOG4CPLUS_DEBUG(logger, LOG4CPLUS_TEXT("header_page->n_data_pages > n_data_pages"));
        uint page_ID = header_page->n_data_pages;
        if(try_insert_record_to_page(hash, page_ID, record, inserted_slot_ID)){
            *inserted_page_ID = page_ID;
            page_manager->release_page(header_page_pool_ID);
            lock_manager->header_unlock(hash);
            lock_manager->data_slot_unlock(hash);
            return;
        }
    }
    else{
        LOG4CPLUS_DEBUG(logger, LOG4CPLUS_TEXT("header_page->n_data_pages <= n_data_pages"));
    }

    DataFilePage* data_page = nullptr;
    int data_page_pool_ID = page_manager->get_page(hash, DATA_TYPE, header_page->n_data_pages, (void**)&data_page);
    memset(data_page, 0, PAGE_SIZE);
    page_manager->flush_page(data_page_pool_ID);
    page_manager->release_page(data_page_pool_ID);

    assert(try_insert_record_to_page(hash, header_page->n_data_pages, record, inserted_slot_ID));
    *inserted_page_ID = header_page->n_data_pages;

    HeaderFilePage* backup_page = nullptr;
    int backup_page_pool_ID = page_manager->get_page(hash, BACKUP_TYPE, 0, (void**)&backup_page);
    backup_page->n_data_pages = header_page->n_data_pages + 1;
    backup_page->n_index_pages = header_page->n_index_pages;
    fill_header_checksum(backup_page);
    page_manager->flush_page(backup_page_pool_ID);
    page_manager->release_page(backup_page_pool_ID);
    header_page->n_data_pages = header_page->n_data_pages + 1;
    fill_header_checksum(header_page);
    page_manager->flush_page(header_page_pool_ID);
    page_manager->release_page(header_page_pool_ID);
    lock_manager->header_unlock(hash);
    lock_manager->data_slot_unlock(hash);
}

/* update_record happen during commit*/
void Executor::update_record(const std::string& hash, uint record_page_ID, uint record_slot_ID, 
                    RecordField field, uint new_value){
    DataFilePage* data_page;
    int data_page_pool_ID = page_manager->get_page(hash, DATA_TYPE, record_page_ID, (void**)&data_page);
    LOG4CPLUS_DEBUG(logger, LOG4CPLUS_TEXT("data_page before update record: ") << data_page->to_str());
    DataRecord old_record = data_page->data_records[record_slot_ID];
    if(field == RECORD_TIMESTAMP){
        data_page->data_records[record_slot_ID].timestamp = new_value;
    }
    else if(field == RECORD_VALUE){
        data_page->data_records[record_slot_ID].value = new_value;
    }
    else{
        assert(0);
    }
    
    LOG4CPLUS_DEBUG(logger, LOG4CPLUS_TEXT("data_page after update record: ") << data_page->to_str());
    log_manager->add_log(DATA_RECORD_UPDATE_LOG, dynamic_transaction_ID, record_page_ID, record_slot_ID * sizeof(DataRecord),
                            sizeof(DataRecord), hash.c_str(), &old_record, &data_page->data_records[record_slot_ID]);
    page_manager->mark_page_dirty(data_page_pool_ID);
    page_manager->release_page(data_page_pool_ID);
}

/* update_index happen during commit*/
void Executor::update_index(const std::string& hash, uint index_page_ID, uint index_slot_ID, 
                    uint record_page_ID, uint record_slot_ID){
    IndexFilePage* index_page = nullptr;
    int index_page_pool_ID = page_manager->get_page(hash, INDEX_TYPE, index_page_ID, (void**)&index_page);
    IndexItem old_item = index_page->index_items[index_slot_ID];
    
    index_page->index_items[index_slot_ID].last_slot_ID = record_slot_ID;
    index_page->index_items[index_slot_ID].last_page_ID = record_page_ID;
    log_manager->add_log(INDEX_ITEM_UPDATE_LOG, dynamic_transaction_ID, index_page_ID, index_slot_ID * sizeof(IndexItem),
                            sizeof(IndexItem), hash.c_str(), &old_item, &index_page->index_items[index_slot_ID]); 
    page_manager->mark_page_dirty(index_page_pool_ID);
    page_manager->release_page(index_page_pool_ID);
}

/* insert_index happen during commit*/
void Executor::insert_index(const std::string& hash, const std::string& key, uint n_index_pages, 
                    uint record_page_ID, uint record_slot_ID){
    lock_manager->index_slot_write_lock(hash);
    if(n_index_pages > 0){
        uint index_page_ID = n_index_pages - 1;
        if(try_insert_index_to_page(hash, key, index_page_ID, record_page_ID, record_slot_ID)){
            lock_manager->index_slot_unlock(hash);
            return;
        }
    }
    lock_manager->header_read_lock(hash);
    HeaderFilePage* header_page = nullptr;
    int header_pool_ID = page_manager->get_page(hash, HEADER_TYPE, 0, (void**)&header_page);
    if(header_page->n_index_pages > n_index_pages){
        uint index_page_ID = header_page->n_index_pages - 1;
        if(try_insert_index_to_page(hash, key, index_page_ID, record_page_ID, record_slot_ID)){
            page_manager->release_page(header_pool_ID);
            lock_manager->header_unlock(hash);
            lock_manager->index_slot_unlock(hash);
            return;
        }
    }
    IndexFilePage* index_page = nullptr;
    uint index_page_ID = header_page->n_index_pages;
    int index_page_pool_ID = page_manager->get_page(hash, INDEX_TYPE, index_page_ID, (void**)&index_page);
    memset(index_page, 0, PAGE_SIZE);
    page_manager->flush_page(index_page_pool_ID);
    page_manager->release_page(index_page_pool_ID);

    HeaderFilePage* backup_page = nullptr;
    int backup_pool_ID = page_manager->get_page(hash, BACKUP_TYPE, 0, (void**)&backup_page);

    backup_page->n_data_pages = header_page->n_data_pages;
    backup_page->n_index_pages = header_page->n_index_pages + 1;
    fill_header_checksum(backup_page);
    page_manager->flush_page(backup_pool_ID);
    page_manager->release_page(backup_pool_ID);
    header_page->n_index_pages = header_page->n_index_pages + 1;
    fill_header_checksum(header_page);
    page_manager->flush_page(header_pool_ID);
    index_page_ID = header_page->n_index_pages - 1;
    assert(try_insert_index_to_page(hash, key, index_page_ID, record_page_ID, record_slot_ID));

    page_manager->release_page(header_pool_ID);
    lock_manager->header_unlock(hash);    
    lock_manager->index_slot_unlock(hash);
}

bool Executor::check_header_valid(const HeaderFilePage* header_page){
    return calc_checksum((const unsigned char*)(header_page),  PAGE_SIZE - CHECKSUM_SIZE) == header_page->check_sum;
}

void Executor::fill_header_checksum(HeaderFilePage* header_page){
    header_page->check_sum = calc_checksum((const unsigned char*)(header_page),  PAGE_SIZE - CHECKSUM_SIZE);
}

bool Executor::get_index_slot_bit(const IndexFilePage* index_page, uint slot_ID){
    assert(slot_ID < N_INDEX_ITEMS_PER_PAGE);
    uint group_ID = slot_ID / 8;
    uint bit_ID = slot_ID % 8;
    unsigned char mask = 1 << bit_ID;
    return index_page->bitmap[group_ID] & mask;
}

void Executor::set_index_slot_bit(IndexFilePage* index_page, uint slot_ID, bool occupy, 
                                    uint* offset, unsigned char* old_value, unsigned char* new_value){
    assert(slot_ID < N_INDEX_ITEMS_PER_PAGE);
    uint group_ID = slot_ID / 8;
    uint bit_ID = slot_ID % 8;
    *offset = PAGE_SIZE - N_INDEX_ITEMS_PER_PAGE / 8 + group_ID;
    unsigned char old = index_page->bitmap[group_ID];
    *old_value = old;
    if(occupy){
        unsigned char mask = 1 << bit_ID;
        index_page->bitmap[group_ID] |= mask;
    }
    else{
        unsigned char mask = 255 - (1 << bit_ID);
        index_page->bitmap[group_ID] &= mask;
    }
    *new_value = index_page->bitmap[group_ID];
}

bool Executor::get_data_slot_bit(const DataFilePage* data_page, uint slot_ID){
    uint group_ID = slot_ID / 8;
    uint bit_ID = slot_ID % 8;
    unsigned char mask = 1 << bit_ID;
    return data_page->bitmap[group_ID] & mask;
}

void Executor::set_data_slot_bit(DataFilePage* data_page, uint slot_ID, bool occupy, uint* offset,
                                unsigned char* old_value, unsigned char* new_value){
    assert(slot_ID < N_DATA_RECORDS_PER_PAGE);
    uint group_ID = slot_ID / 8;
    uint bit_ID = slot_ID % 8;
    *offset = PAGE_SIZE - N_DATA_RECORDS_PER_PAGE / 8 + group_ID;
    unsigned char old = data_page->bitmap[group_ID];
    *old_value = old;
    if(occupy){
        unsigned char mask = 1 << bit_ID;
        data_page->bitmap[group_ID] |= mask;
    }
    else{
        unsigned char mask = 255 - (1 << bit_ID);
        data_page->bitmap[group_ID] &= mask;
    }
    *new_value = data_page->bitmap[group_ID];
}

void Executor::fill_index_item(IndexItem* index_item, const std::string& key, uint last_page_ID, uint last_slot_ID){
    strncpy(index_item->key, key.c_str(), sizeof(index_item->key));
    index_item->last_page_ID = last_page_ID;
    index_item->last_slot_ID = last_slot_ID;
}

bool Executor::try_insert_index_to_page(const std::string& hash, const std::string& key, uint index_page_ID, 
                    uint record_page_ID, uint record_slot_ID){
    IndexFilePage* index_page = nullptr;
    int index_page_pool_ID = page_manager->get_page(hash, INDEX_TYPE, index_page_ID, (void**)&index_page);
    for(uint slot_ID = 0; slot_ID < N_INDEX_ITEMS_PER_PAGE; slot_ID++){
        bool occupy = get_index_slot_bit(index_page, slot_ID);
        if(!occupy){
            uint offset;
            unsigned char old_value;
            unsigned char new_value;
            set_index_slot_bit(index_page, slot_ID, true, &offset, &old_value, &new_value);
            log_manager->add_log(INDEX_SLOT_LOG, dynamic_transaction_ID, index_page_ID, offset, sizeof(unsigned char),
                                    hash.c_str(), &old_value, &new_value);
            IndexItem old_item = index_page->index_items[slot_ID];
            fill_index_item(&index_page->index_items[slot_ID], key, record_page_ID, record_slot_ID);
            log_manager->add_log(INDEX_ITEM_INSERT_LOG, dynamic_transaction_ID, index_page_ID, slot_ID * sizeof(IndexItem),
                        sizeof(IndexItem), hash.c_str(), &old_item, &index_page->index_items[slot_ID]); 
            page_manager->mark_page_dirty(index_page_pool_ID);
            page_manager->release_page(index_page_pool_ID);
            return true;
        }
    }
    page_manager->release_page(index_page_pool_ID);
    return false;
}

bool Executor::try_insert_record_to_page(const std::string& hash, uint page_ID, const DataRecord& record, uint* inserted_slot_ID){
    LOG4CPLUS_DEBUG(logger, LOG4CPLUS_TEXT("try_insert_record_to_page"));
    DataFilePage* data_page = nullptr;
    int data_page_pool_ID = page_manager->get_page(hash, DATA_TYPE, page_ID, (void**)&data_page);
    LOG4CPLUS_INFO(logger, LOG4CPLUS_TEXT("data_page before insert: ") << data_page->to_str());
    for(uint slot_ID = 0; slot_ID < N_DATA_RECORDS_PER_PAGE; slot_ID++){
        bool occupy = get_data_slot_bit(data_page, slot_ID);
        if(!occupy){
            uint offset;
            unsigned char old_value;
            unsigned char new_value;
            set_data_slot_bit(data_page, slot_ID, true, &offset, &old_value, &new_value);
            log_manager->add_log(DATA_SLOT_LOG, dynamic_transaction_ID, page_ID, offset, sizeof(unsigned char),
                                    hash.c_str(), &old_value, &new_value);
            DataRecord old_record = data_page->data_records[slot_ID];
            data_page->data_records[slot_ID] = record;
            LOG4CPLUS_INFO(logger, LOG4CPLUS_TEXT("data_page after insert: ") << data_page->to_str());            
            log_manager->add_log(DATA_RECORD_INSERT_LOG, dynamic_transaction_ID, page_ID, slot_ID * sizeof(DataRecord),
                                    sizeof(DataRecord), hash.c_str(), &old_record, &record);
            page_manager->mark_page_dirty(data_page_pool_ID);
            page_manager->release_page(data_page_pool_ID);
            *inserted_slot_ID = slot_ID;
            return true;
        }
    }
    page_manager->release_page(data_page_pool_ID);
    return false;
}