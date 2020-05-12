#pragma once
#include <string>
#include <staydb/logmanager/logmanager.h>
#include <staydb/pagemanager/poolmanager.h>

class PageManager{
public:
    static PageManager* get_instance();
    int get_page(const std::string& hash, const std::string& type, uint page_ID, void** page);
    void release_page(int pool_page_ID);
    void flush_page(int pool_page_ID);
    void mark_page_dirty(int pool_page_ID);
    void flush_all_pages();
    int get_log_page(const std::string& type, uint page_ID, void** page);
    void release_log_page(int pool_page_ID);
    void flush_log_page(int pool_page_ID);
    void mark_log_page_dirty(int pool_page_ID);
    void flush_all_log_pages();
    void set_log_manager(LogManager* _log_manager);

private:
    PageManager();
    ~PageManager();
    static PageManager* instance;
    static LogManager* log_manager;
    PoolManager* data_pool_manager;
    PoolManager* log_pool_manager;
    static bool flush_log_data_pool();
    static bool flush_log_log_pool();
    std::string hash_to_dir_path(const std::string& hash);
};