#include <staydb/logmanager/logmanager.h>
#include <staydb/util/crc32.h>
#include <cstring>
#include <cassert>


LogManager* LogManager::instance = nullptr;

LogManager::LogManager(){
    logger = log4cplus::Logger::getInstance(LOG4CPLUS_TEXT("logmanager"));
    pthread_mutex_init(&mutex, 0);
    page_manager = PageManager::get_instance();
    lock_manager = LockManager::get_instance();
    LOG4CPLUS_DEBUG(logger, LOG4CPLUS_TEXT("init lock manager"));
    header_page_pool_ID = page_manager->get_log_page(HEADER_TYPE, 0, (void**)&header_page);
    LOG4CPLUS_DEBUG(logger, LOG4CPLUS_TEXT("header_page_pool_ID: ") << header_page_pool_ID);
    backup_page_pool_ID = page_manager->get_log_page(BACKUP_TYPE, 0, (void**)&backup_page);
    LOG4CPLUS_DEBUG(logger, LOG4CPLUS_TEXT("backup_page_pool_ID: ") << backup_page_pool_ID);
    next_log_ID = 0;
    if(!check_header_valid(header_page)){
        if(check_header_valid(backup_page)){
            LOG4CPLUS_DEBUG(logger, LOG4CPLUS_TEXT("backup page valie"));
            memcpy(header_page, backup_page, PAGE_SIZE);
            page_manager->flush_log_page(header_page_pool_ID);
        }
        else{
            LOG4CPLUS_DEBUG(logger, LOG4CPLUS_TEXT("backup page invalid"));
            backup_page->n_log_pages = 0;
            fill_header_checksum(backup_page);
            assert(check_header_valid(backup_page));
            page_manager->flush_log_page(backup_page_pool_ID);
            memcpy(header_page, backup_page, PAGE_SIZE);
            page_manager->flush_log_page(header_page_pool_ID);
        }
    }
    LOG4CPLUS_INFO(logger, LOG4CPLUS_TEXT("header_page->n_log_pages: ") << header_page->n_log_pages);
    if(header_page->n_log_pages == 0){
        log_page_pool_ID = page_manager->get_log_page(LOG_TYPE, 0, (void**)&log_page);
        memset(log_page, 0, PAGE_SIZE);
        current_log_page_ever_flushed = false;
    }
    else{
        log_page_pool_ID = page_manager->get_log_page(LOG_TYPE, header_page->n_log_pages - 1, (void**)&log_page);
        current_log_page_ever_flushed = true;
    }

}

LogManager* LogManager::get_instance(){
    if(instance == nullptr){
        instance = new LogManager();
    }
    return instance;
}

LogItem LogManager::add_log(LogType log_type, uint transaction_ID, 
                uint page_ID, uint offset, uint length, const char* hash, const void* old_value, const void* new_value){
    pthread_mutex_lock(&mutex);
    LogItem log_item = build_normal_log_item(log_type, transaction_ID, page_ID, offset, length, 
                                                hash, old_value, new_value);
    add_log_item_to_page(log_item);
    pthread_mutex_unlock(&mutex);
}

void LogManager::undo_logs(std::vector<LogItem> log_items){
    
    for(LogItem log_item: log_items){
        if(log_item.log_type == DATA_SLOT_LOG){
            DataFilePage* data_page;
            lock_manager->data_slot_write_lock(log_item.hash);
            int data_page_pool_ID = page_manager->get_page(log_item.hash, DATA_TYPE, log_item.page_ID, (void**)&data_page);
            char* byte_to_change = (char*)(data_page) + log_item.offset;
            unsigned mask = UINT8_MAX - (log_item.new_value[0] ^ log_item.old_value[0]);
            *byte_to_change = (*byte_to_change) & mask;
            lock_manager->data_slot_unlock(log_item.hash);
            
            pthread_mutex_lock(&mutex);
            LogItem undo_log_item = build_undo_log_item(log_item);
            add_log_item_to_page(undo_log_item);
            pthread_mutex_unlock(&mutex);
            page_manager->mark_page_dirty(data_page_pool_ID);
            page_manager->release_page(data_page_pool_ID);
        }
        else if(log_item.log_type == INDEX_SLOT_LOG){
            IndexFilePage* index_page;
            lock_manager->index_slot_write_lock(log_item.hash);
            int index_page_pool_ID = page_manager->get_page(log_item.hash, INDEX_TYPE, log_item.page_ID, (void**)&index_page);
            char* byte_to_change = (char*)(index_page) + log_item.offset;
            unsigned mask = UINT8_MAX - (log_item.new_value[0] ^ log_item.old_value[0]);
            *byte_to_change = (*byte_to_change) & mask;
            lock_manager->index_slot_unlock(log_item.hash);

            pthread_mutex_lock(&mutex);
            LogItem undo_log_item = build_undo_log_item(log_item);
            add_log_item_to_page(undo_log_item);
            pthread_mutex_unlock(&mutex);
            page_manager->mark_page_dirty(index_page_pool_ID);
            page_manager->release_page(index_page_pool_ID);
        }
        else if(log_item.log_type == INDEX_ITEM_UPDATE_LOG){
            IndexFilePage* index_page;
            int index_page_pool_ID = page_manager->get_page(log_item.hash, INDEX_TYPE, log_item.page_ID, (void**)&index_page);
            char* byte_to_change = (char*)(index_page) + log_item.offset;
            memcpy(byte_to_change, log_item.old_value, log_item.length);

            pthread_mutex_lock(&mutex);
            LogItem undo_log_item = build_undo_log_item(log_item);
            add_log_item_to_page(undo_log_item);
            pthread_mutex_unlock(&mutex);
            page_manager->mark_page_dirty(index_page_pool_ID);
            page_manager->release_page(index_page_pool_ID);
        }
        // DATA_RECORD_INSERT_LOG,
        // DATA_RECORD_UPDATE_LOG,
        // INDEX_ITEM_INSERT_LOG,
        // INDEX_ITEM_UPDATE_LOG,
        // DATA_RECORD_UNDO_LOG,
        // DATA_SLOT_UNDO_LOG,
        // INDEX_ITEM_UNDO_LOG,
        // INDEX_SLOT_UNDO_LOG,
        // COMMIT_LOG,
        // ABORT_LOG,
        // CHECKPOINT_LOG    
    }
    
}

void LogManager::recover(){
    pthread_mutex_lock(&mutex);
    /* TODO */
    pthread_mutex_unlock(&mutex);
}

void LogManager::flush_logs(){
    pthread_mutex_lock(&mutex);
    _flush_logs();
    pthread_mutex_unlock(&mutex);
}

void LogManager::_flush_logs(){
    flush_log_page_with_backup();
    if(!current_log_page_ever_flushed){
        current_log_page_ever_flushed = true;
        header_page->n_log_pages++;
        flush_header_page_with_backup();
    }
}

bool LogManager::check_header_valid(const LogHeaderFilePage* _header_page){
    return calc_checksum((const unsigned char*)(_header_page),  PAGE_SIZE - CHECKSUM_SIZE) == _header_page->check_sum;
}

void LogManager::fill_header_checksum(LogHeaderFilePage* _header_page){
    _header_page->check_sum = calc_checksum((const unsigned char*)(_header_page),  PAGE_SIZE - CHECKSUM_SIZE);
}

bool LogManager::check_log_page_valid(const LogFilePage* _log_page){
    return calc_checksum((const unsigned char*)_log_page, PAGE_SIZE - CHECKSUM_SIZE) == _log_page->check_sum;
}

void LogManager::fill_log_page_checksum(LogFilePage* _log_page){
    _log_page->check_sum = calc_checksum((const unsigned char*)(_log_page), PAGE_SIZE - CHECKSUM_SIZE);
}


uint LogManager::calc_checksum(const unsigned char *buf, int len){
    return xcrc32(buf, len, 0xffffffff);
}

bool LogManager::get_log_slot_bit(const LogFilePage* log_page, uint slot_ID){
    assert(slot_ID < N_LOG_ITEMS_PER_PAGE);
    uint group_ID = slot_ID / 8;
    uint bit_ID = slot_ID % 8;
    unsigned char mask = 1 << bit_ID;
    return log_page->bitmap[group_ID] & mask;
}

void LogManager::set_log_slot_bit(LogFilePage* log_page, uint slot_ID){
    assert(slot_ID < N_LOG_ITEMS_PER_PAGE);
    uint group_ID = slot_ID / 8;
    uint bit_ID = slot_ID % 8;
    unsigned char mask = 1 << bit_ID;
    log_page->bitmap[group_ID] |= mask;
}

LogItem LogManager::build_normal_log_item(LogType log_type, uint transaction_ID, 
                uint page_ID, uint offset, uint length, const char* hash, const void* old_value, const void* new_value){
    LogItem item;
    item.log_type = log_type;
    item.transaction_ID = transaction_ID;
    item.page_ID = page_ID;
    item.offset = offset;
    item.length = length;
    item.log_ID = next_log_ID++;
    if(hash != nullptr){
        strncpy(item.hash, hash, HASH_LENGTH + 1);
    }
    if(old_value != nullptr){
        assert(length != 0);
        memcpy(item.old_value, old_value, length);
    }
    if(new_value != nullptr){
        assert(length != 0);
        memcpy(item.new_value, new_value, length);
    }
}

LogItem LogManager::build_undo_log_item(const LogItem& log_item){
    LogItem undo_log_item;
    if(log_item.log_type == DATA_SLOT_LOG){
        undo_log_item.log_type = DATA_SLOT_UNDO_LOG;
    }
    else if(log_item.log_type == INDEX_SLOT_LOG){
        undo_log_item.log_type = INDEX_SLOT_UNDO_LOG;
    }
    else if(log_item.log_type == INDEX_ITEM_UPDATE_LOG){
        undo_log_item.log_type = INDEX_ITEM_UNDO_LOG;
    }
    else{
        assert(0);
    }
    undo_log_item.transaction_ID = log_item.transaction_ID;
    undo_log_item.page_ID = log_item.page_ID;
    undo_log_item.offset = log_item.offset;
    undo_log_item.length = log_item.length;
    undo_log_item.log_ID = next_log_ID++;
    undo_log_item.undo_log_ID = log_item.log_ID;
    strncpy(undo_log_item.hash, log_item.hash, HASH_LENGTH + 1);
    memcpy(undo_log_item.new_value, log_item.old_value, log_item.length);
    memcpy(undo_log_item.old_value, log_item.new_value, log_item.length);
    //memcpy(&undo_log_item.
}

void LogManager::flush_log_page_with_backup(){
    fill_log_page_checksum(log_page);
    memcpy(backup_page, log_page, PAGE_SIZE);
    page_manager->flush_log_page(backup_page_pool_ID);
    page_manager->flush_log_page(log_page_pool_ID);
}

void LogManager::flush_header_page_with_backup(){
    fill_header_checksum(header_page);
    memcpy(backup_page, header_page, PAGE_SIZE);
    page_manager->flush_log_page(backup_page_pool_ID);
    page_manager->flush_log_page(header_page_pool_ID);
}

void LogManager::add_log_item_to_page(const LogItem& log_item){
    uint slot_ID;
    for(slot_ID = 0; slot_ID < N_LOG_ITEMS_PER_PAGE; slot_ID++){
        bool occupy = get_log_slot_bit(log_page, slot_ID);
        if(!occupy){
            break;
        }
    }
    if(slot_ID == N_LOG_ITEMS_PER_PAGE){
        _flush_logs();
        page_manager->release_log_page(log_page_pool_ID);
        log_page_pool_ID = page_manager->get_log_page(LOG_TYPE, header_page->n_log_pages, (void**)&log_page);
        memset(log_page, 0, PAGE_SIZE);
        current_log_page_ever_flushed = false;
        slot_ID = 0;
    }
    log_page->log_items[slot_ID] = log_item;
    set_log_slot_bit(log_page, slot_ID);
}
    