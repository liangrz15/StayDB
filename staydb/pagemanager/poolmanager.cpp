#include <staydb/pagemanager/poolmanager.h>
#include <cassert>
#include <staydb/util/file_util.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>

PoolManager::PoolManager(uint _n_pages, uint _page_size, uint _max_n_fds){
    logger = log4cplus::Logger::getInstance(LOG4CPLUS_TEXT("poolmanager"));
    n_pages = _n_pages;
    page_size = _page_size;
    max_n_fds = _max_n_fds;
    pthread_mutex_init(&mutex, 0);
    free_bitmap = new bool[n_pages];
    for(uint i = 0; i < n_pages; i++){
        free_bitmap[i] = true;
    }
    page_metadata_array = new PageMetadata[n_pages];
    buf = new char[n_pages * page_size];
}

PoolManager::~PoolManager(){
    delete[] free_bitmap;
    delete[] page_metadata_array;
    delete[] buf;
}

int PoolManager::get_page(const std::string& dir_path, const std::string& base_name, uint page_ID, void** page, std::function<bool()> flush_log){
    pthread_mutex_lock(&mutex);

    std::string file_path = join_path(dir_path, base_name);
    LOG4CPLUS_DEBUG(logger, LOG4CPLUS_TEXT("file_path: ") << file_path);
    if(file_metadata.find(file_path) == file_metadata.end()){
        LOG4CPLUS_DEBUG(logger, LOG4CPLUS_TEXT("not find file_path: ") << file_path);
        if(file_metadata.size() >= max_n_fds){
            assert(clean_pool(flush_log));
            assert(file_metadata.size() < max_n_fds);
        }
        mkdir_recursive(dir_path.c_str());
        create_file(file_path.c_str());
        int fd = open(file_path.c_str(), O_RDWR);
        LOG4CPLUS_DEBUG(logger, LOG4CPLUS_TEXT("fd: ") << fd);
        assert(fd > 0);
        file_metadata[file_path] = FileMetadata(fd);
    }
    else{
        LOG4CPLUS_DEBUG(logger, LOG4CPLUS_TEXT("find file_path: ") << file_path);
    }

    FilePagePair file_page_pair = FilePagePair(file_path, page_ID);
    uint page_pool_ID;
    if(filepage_to_pool_ID.find(file_page_pair) == filepage_to_pool_ID.end()){
        LOG4CPLUS_DEBUG(logger, LOG4CPLUS_TEXT("not find file_page_pair: ") << file_page_pair.file_name << file_page_pair.page_ID);
        file_metadata[file_path].n_pages++;
        if(!get_free_pool_page(&page_pool_ID)){
            assert(clean_pool(flush_log));
            assert(get_free_pool_page(&page_pool_ID));
        }
        free_bitmap[page_pool_ID] = false;
        filepage_to_pool_ID[file_page_pair] = page_pool_ID;
        PageMetadata& page_metadata = page_metadata_array[page_pool_ID];
        page_metadata.reset(file_path, page_ID);
        read_page_from_file(file_metadata[file_path].fd, page_ID, buf + page_pool_ID * page_size);
    }
    else{
        LOG4CPLUS_DEBUG(logger, LOG4CPLUS_TEXT("find file_page_pair: ") << file_page_pair.file_name << file_page_pair.page_ID);
        page_pool_ID = filepage_to_pool_ID[file_page_pair];
        LRU_queue.erase(page_metadata_array[page_pool_ID].it_LRU);
    }
    
    PageMetadata& page_metadata = page_metadata_array[page_pool_ID];
    page_metadata.n_readers++;
    LOG4CPLUS_DEBUG(logger, LOG4CPLUS_TEXT("page_metadata: ") << page_metadata.to_str());
    LRU_queue.push_back(LRUNode(page_pool_ID));
    auto it = LRU_queue.end();
    it--;
    page_metadata.it_LRU = it;
    *page = buf + page_pool_ID * page_size;

    pthread_mutex_unlock(&mutex);
    LOG4CPLUS_DEBUG(logger, LOG4CPLUS_TEXT("page_pool_ID: ") << page_pool_ID);
    return page_pool_ID;
}

void PoolManager::release_page(int pool_page_ID){
    pthread_mutex_lock(&mutex);
    page_metadata_array[pool_page_ID].n_readers--;
    pthread_mutex_unlock(&mutex);
}

void PoolManager::flush_page(int pool_page_ID){
    pthread_mutex_lock(&mutex);
    _flush_page(pool_page_ID);
    pthread_mutex_unlock(&mutex);
}

void PoolManager::_flush_page(int pool_page_ID){
    assert(!free_bitmap[pool_page_ID]);
    PageMetadata& page_metadata = page_metadata_array[pool_page_ID];
    int fd = file_metadata[page_metadata.file_name].fd;
    lseek(fd, page_metadata.page_ID * page_size, SEEK_SET);
    write(fd, buf + pool_page_ID * page_size, page_size);
    fsync(fd);
    page_metadata.dirty = false;
}

void PoolManager::mark_page_dirty(int pool_page_ID){
    pthread_mutex_lock(&mutex);
    page_metadata_array[pool_page_ID].dirty = true;
    pthread_mutex_unlock(&mutex);
}

void PoolManager::flush_all_pages(){
    pthread_mutex_lock(&mutex);
    for(uint i = 0; i < n_pages; i++){
        if(!free_bitmap[i]){
            if(page_metadata_array[i].dirty){
                _flush_page(i);
            }
        }
    }
    pthread_mutex_unlock(&mutex);
}

bool PoolManager::clean_pool(std::function<bool()> flush_log){
    while(true){
        auto it = LRU_queue.begin();
        assert(it != LRU_queue.end());
        uint page_pool_ID = it->page_pool_ID;
        const PageMetadata& page_metadata = page_metadata_array[page_pool_ID];
        if(page_metadata.n_readers == 0 && !page_metadata.dirty){
            clean_pool_pos(page_pool_ID);
            if(file_metadata.size() < max_n_fds){
                return true;
            }
        }
        else{
            break;
        }
    }
    if(!flush_log()){
        return false;
    }
    int n_cleaned_page = 0;
    for(uint i = 0; i < n_pages; i++){
        if(!free_bitmap[i]){
            const PageMetadata& page_metadata = page_metadata_array[i];
            if(page_metadata.n_readers == 0){
                n_cleaned_page++;
                if(page_metadata.dirty){
                    _flush_page(i);
                }
                clean_pool_pos(i);
            }
        }
    }
    assert(n_cleaned_page > 0);
    assert(file_metadata.size() < max_n_fds);
    return true;
}

void PoolManager::clean_pool_pos(uint page_pool_ID){
    const PageMetadata& page_metadata = page_metadata_array[page_pool_ID];
    free_bitmap[page_pool_ID] = true;
    const std::string& file_name = page_metadata.file_name;
    file_metadata[file_name].n_pages--;
    if(file_metadata[file_name].n_pages == 0){
        close(file_metadata[file_name].fd);
        file_metadata.erase(file_name);
    }
    FilePagePair pair = FilePagePair(file_name, page_metadata.page_ID);
    filepage_to_pool_ID.erase(pair);
    LRU_queue.erase(page_metadata.it_LRU);
}

bool PoolManager::get_free_pool_page(uint* page_pool_ID){
    for(uint i = 0; i < n_pages; i++){
        if (free_bitmap[i]){
            *page_pool_ID = i;
            return true;
        }
    }
    return false;
}

void PoolManager::read_page_from_file(int fd, uint page_ID, void* page_buf){
    lseek(fd, page_ID * page_size, SEEK_SET);
    int size = read(fd, page_buf, page_size);
    assert(size >= 0);
    if((uint)size < page_size){
        memset(page_buf, 0, page_size);
    }
}