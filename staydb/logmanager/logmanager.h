#pragma once
#include <staydb/filelayout.h>
#include <vector>
#include <staydb/pagemanager/pagemanager.h>
#include <pthread.h>
#include <staydb/lockmanager/lockmanager.h>
#include <log4cplus/loggingmacros.h>
#include <log4cplus/logger.h>

class LogManager{
public:
    static LogManager* get_instance();
    LogItem add_log(LogType log_type, uint transaction_ID, 
                uint page_ID, uint offset, uint length, const char* hash, const void* old_value, const void* new_value);
    void add_checkpoint_log();
    void undo_logs(const std::vector<LogItem>& log_items);
    void recover(uint* max_transaction_ID, uint* max_timestamp);
    void flush_logs();

private:
    log4cplus::Logger logger;
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
    LogItem build_checkpoint_log_item();
    void flush_log_page_with_backup();
    void flush_header_page_with_backup();
    void _flush_logs();
    void add_log_item_to_page(const LogItem& log_item);

    void backtrace_until_checkpoint(std::list<LogFilePage>* log_pages, uint* first_slot_ID_after_checkpoint,
                                    uint* max_transaction_ID, uint* max_timestamp);
    void redo_pass(const std::list<LogFilePage>& log_pages, uint first_slot_ID_after_checkpoint,
                std::map<uint, std::vector<LogItem>>* transaction_ID_to_logitems_to_undo, uint* max_transaction_ID, uint* max_timestamp);

    void undo_pass(const std::map<uint, std::vector<LogItem>>& transaction_ID_to_logitems_to_undo);

    void get_log_page_and_realease(uint page_ID, LogFilePage* log_page);
    void redo_logitem(const LogItem& log_item);
    static void update_logitems_to_undo(const LogItem& log_item, 
        std::map<uint, std::vector<LogItem>>* _transaction_ID_to_logitems_to_undo);
    static void update_max_transaction_ID_with_logitem(const LogItem& log_item, uint* max_transaction_ID);
    static void update_max_timestamp_with_logitem(const LogItem& log_item, uint* max_timestamp);
    static bool is_data_log_type(LogType log_type);
    static bool is_index_log_type(LogType log_type);
    static bool log_type_need_undo(LogType log_type);
    static LogType get_undo_log_type(LogType log_type);
    static bool is_undo_log_type(LogType log_type);
};