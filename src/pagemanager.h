#pragma once
#include <string>

class PageManager{
public:
    int get_page(const std::string& filename, uint page_ID, bool pool, char* page);
    void release_page(int page_key, bool pool);
    void flush_page(int page_key, bool pool);
    void mark_dirty(int page_key, bool pool);
    void flush_pool_pages();
    std::string get_file_name(const std::string& base_name, const std::string& suffix);
};