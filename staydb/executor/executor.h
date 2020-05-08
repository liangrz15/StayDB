#pragma once
#include <string>
#include <staydb/const.h>
#include <staydb/lockmanager/lockmanager.h>
#include <staydb/filelayout.h>
#include <staydb/logmanager/logmanager.h>
#include <vector>

enum executor_error_t{
    ERROR_NONE,
    ERROR_WAIT_WRITE,
    ERROR_RETRY_WRITE,
    ERROR_INSERT_EXIST,
    ERROR_READ_NOT_EXIST,
    ERROR_WRITE_NOT_EXIST
};

struct WriteItem{
    WriteItem(const std::string& _hash, const std::string& _key, uint _inserted_page_ID, 
                uint _inserted_slot_ID, uint _n_index_pages, bool _first_record = true, uint _index_page_ID = 0,
                uint _index_slot_ID = 0): hash(_hash), key(_key), inserted_page_ID(_inserted_page_ID),
                inserted_slot_ID(_inserted_slot_ID), n_index_pages(_n_index_pages), first_record(_first_record), 
                index_page_ID(_index_page_ID), index_slot_ID(_index_slot_ID) {}
    std::string hash;
    std::string key;
    uint inserted_page_ID;
    uint inserted_slot_ID;
    uint n_index_pages;
    bool first_record;
    uint index_page_ID;
    uint index_slot_ID;
};


/* executor for read/write/insert/delete command */
class Executor{

public:
    Executor();
    executor_error_t read_key(const std::string& key, int* value);
    executor_error_t write_key(const std::string& key, int value, uint* wait_transaction_ID);
    executor_error_t insert_key(const std::string& key, int value, uint* wait_transaction_ID);
    executor_error_t delete_key(const std::string& key, uint* wait_transaction_ID);
    executor_error_t abort();
    executor_error_t commit();
    void reset(uint static_transaction_ID);

private:
    LockManager* lock_manager;
    LogManager* log_manager;
    uint static_transaction_ID;
    uint dynamic_transaction_ID;
    uint read_timestamp;
    uint write_timestamp;
    std::vector<LogItem> log_items;
    std::vector<WriteItem> write_items;
    std::string calc_hash(const std::string& key);
    uint calc_checksum(const unsigned char *buf, int len);

    void parse_header(const std::string& hash, uint* n_index_pages, uint* n_data_pages);
    bool find_index(const std::string& hash, const std::string& key, uint n_index_pages, uint* index_page_ID, 
                    uint* index_slot_ID, uint* record_page_ID, uint* record_slot_ID);
    DataRecord get_record(const std::string& hash, uint record_page_ID, uint record_slot_ID);
    void insert_record(const std::string& hash, uint n_data_pages, const DataRecord& record, 
                        uint* inserted_page_ID, uint* inserted_slot_ID);

    /* update_record happen during commit*/
    void update_record(const std::string& hash, uint record_page_ID, uint record_slot_ID, 
                        uint write_timestamp);
    /* update_index happen during commit*/
    void update_index(const std::string& hash, uint index_page_ID, uint index_slot_ID, 
                        uint record_page_ID, uint record_slot_ID);
    /* insert_index happen during commit*/
    void insert_index(const std::string& hash, const std::string& key, uint n_index_pages, 
                        uint record_page_ID, uint record_slot_ID);



};