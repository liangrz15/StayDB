#pragma once
#include <string>

class PageManager{
public:
    int get_page(const std::string& hash, const std::string& type, uint page_ID, char* page);
    void release_page(int pool_page_ID);
    void flush_page(int pool_page_ID);
    void mark_page_dirty(int pool_page_ID);
    void flush_all_pages();
    int get_log_page(const std::string& type, uint page_ID, char* page);
    void release_log_page(int pool_page_ID);
    void flush_log_page(int pool_page_ID);
    void mark_log_page_dirty(int pool_page_ID);
    void flush_all_log_pages();
};