#include <staydb/pagemanager/pagemanager.h>
#include <staydb/logmanager/logmanager.h>
#include <staydb/const.h>

PageManager* PageManager::instance = nullptr;

PageManager::PageManager(){
    data_pool_manager = new PoolManager(DATA_POOL_SIZE, PAGE_SIZE, DATA_POOL_MAX_FD);
    log_pool_manager = new PoolManager(LOG_POOL_SIZE, PAGE_SIZE, LOG_POOL_MAX_FD);
}
PageManager::~PageManager(){
    delete data_pool_manager;
    delete log_pool_manager;
}

PageManager* PageManager::get_instance(){
    if(instance == nullptr){
        instance = new PageManager();
    }
    return instance;
}


int PageManager::get_page(const std::string& hash, const std::string& type, uint page_ID, void** page){
    const std::string& dir_name = std::string("db/") + hash_to_dir_path(hash);
    return data_pool_manager->get_page(dir_name, type, page_ID, page, flush_log_data_pool);
}
void PageManager::release_page(int pool_page_ID){
    data_pool_manager->release_page(pool_page_ID);
}

void PageManager::flush_page(int pool_page_ID){
    data_pool_manager->flush_page(pool_page_ID);
}

void PageManager::mark_page_dirty(int pool_page_ID){
    data_pool_manager->mark_page_dirty(pool_page_ID);
}

void PageManager::flush_all_pages(){
    data_pool_manager->flush_all_pages();
}

int PageManager::get_log_page(const std::string& type, uint page_ID, void** page){
    const std::string dir_name = "db/log";
    return log_pool_manager->get_page(dir_name, type, page_ID, page, flush_log_log_pool);
}

void PageManager::release_log_page(int pool_page_ID){
    log_pool_manager->release_page(pool_page_ID);
}

void PageManager::flush_log_page(int pool_page_ID){
    log_pool_manager->flush_page(pool_page_ID);
}

void PageManager::mark_log_page_dirty(int pool_page_ID){
    log_pool_manager->mark_page_dirty(pool_page_ID);
}

void PageManager::flush_all_log_pages(){
    log_pool_manager->flush_all_pages();
}

bool PageManager::flush_log_data_pool(){
    LogManager::get_instance()->flush_logs();
    return true;
}

bool PageManager::flush_log_log_pool(){
    return false;
}

std::string PageManager::hash_to_dir_path(const std::string& hash){
    return hash;
}
    