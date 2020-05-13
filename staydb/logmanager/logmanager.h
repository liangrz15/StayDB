#pragma once
#include <staydb/filelayout.h>
#include <vector>
#include <staydb/pagemanager/pagemanager.h>
#include <pthread.h>
#include <staydb/lockmanager/lockmanager.h>

class LogManager{
public:
    static LogManager* get_instance();
    LogItem add_log(LogType log_type, uint transaction_ID, 
                uint page_ID, uint offset, uint length, const char* hash, const void* old_value, const void* new_value);
    void undo_logs(std::vector<LogItem> log_items);
    void recover();
    void flush_logs();

private:
    LogManager();
    PageManager* page_manager;
    LockManager* lock_manager;
    static LogManager* instance;
    int header_page_pool_ID;
    LogHeaderFilePage* header_page;
    int backup_page_pool_ID;
    LogHeaderFilePage* backup_page;
    int log_page_pool_ID;
    LogFilePage* log_page;
    bool current_log_page_ever_flushed;
    uint next_log_ID;
    pthread_mutex_t mutex;
    bool check_header_valid(const LogHeaderFilePage* header_page);
    void fill_header_checksum(LogHeaderFilePage* header_page);
    bool check_log_page_valid(const LogFilePage* log_page);
    void fill_log_page_checksum(LogFilePage* log_page);
    uint calc_checksum(const unsigned char *buf, int len);
    static bool get_log_slot_bit(const LogFilePage* log_page, uint slot_ID);
    static void set_log_slot_bit(LogFilePage* log_page, uint slot_ID);
    LogItem build_normal_log_item(LogType log_type, uint transaction_ID, 
                uint page_ID, uint offset, uint length, const char* hash, const void* old_value, const void* new_value);
    LogItem build_undo_log_item(const LogItem& log_item);
    void flush_log_page_with_backup();
    void flush_header_page_with_backup();
    void _flush_logs();
    void add_log_item_to_page(const LogItem& log_item);
};