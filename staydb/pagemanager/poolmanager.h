#pragma once
#include <string>
#include <staydb/const.h>
#include <list>
#include <map>
#include <functional>
#include <pthread.h>
#include <log4cplus/loggingmacros.h>
#include <log4cplus/logger.h>


struct LRUNode{
    LRUNode(uint _page_pool_ID): page_pool_ID(_page_pool_ID){}
    uint page_pool_ID;
};

struct PageMetadata{
    bool dirty;
    int n_readers;
    uint page_ID;
    std::string file_name;
    std::list<LRUNode>::iterator it_LRU;
    void reset(const std::string& _file_name, uint _page_ID){
        file_name = _file_name;
        page_ID = _page_ID;
        dirty = false;
        n_readers = 0;
    }
    std::string to_str() const{
        std::string str = std::string("dirty: ") + std::to_string(dirty) + std::string(", n_readers: ") + std::to_string(n_readers)
                            + std::string(", page_ID: ") + std::to_string(page_ID) + std::string(", file_name: ") + file_name;
        return str;
    }
};

struct FileMetadata{
    FileMetadata(int _fd = 0): fd(_fd), n_pages(0){}
    int fd;
    int n_pages;
};

struct FilePagePair{
    FilePagePair(const std::string& _file_name, uint _page_ID): file_name(_file_name), page_ID(_page_ID){} 
    std::string file_name;
    uint page_ID;
    bool operator<(const FilePagePair& rhs) const{
        if(file_name == rhs.file_name){
            return page_ID < rhs.page_ID;
        }
        else{
            return file_name < rhs.file_name;
        }
    }
};


class PoolManager{
public:
    PoolManager(uint _n_pages, uint _page_size, uint _max_n_fds);
    ~PoolManager();
    int get_page(const std::string& dir_path, const std::string& base_name, uint page_ID, void** page, std::function<bool()> flush_log);
    void release_page(int pool_page_ID);
    void flush_page(int pool_page_ID);
    void mark_page_dirty(int pool_page_ID);
    void flush_all_pages();

private:
    log4cplus::Logger logger;
    pthread_mutex_t mutex;
    uint n_pages;
    uint page_size;
    uint max_n_fds;
    bool* free_bitmap;
    PageMetadata* page_metadata_array;
    char* buf;
    std::map<std::string, FileMetadata> file_metadata;
    std::list<LRUNode> LRU_queue;
    std::map<FilePagePair, uint> filepage_to_pool_ID;
    static std::string join_path(const std::string& dir_path, const std::string& base_name){
        return dir_path + std::string("/") + base_name;
    }

    bool clean_pool(std::function<bool()> flush_log); // clean the unused pages and file descriptors
    bool get_free_pool_page(uint* page_pool_ID);
    void read_page_from_file(int fd, uint page_ID, void* page_buf);
    void clean_pool_pos(uint page_pool_ID);
    void _flush_page(int pool_page_ID);
};