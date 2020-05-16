#pragma once
#include "const.h"
#include <string>

const int HEADER_FILE_PADDING_LENGTH = PAGE_SIZE - 2 * sizeof(uint) - CHECKSUM_SIZE;
struct HeaderFilePage{
    uint n_index_pages;
    uint n_data_pages;
    char padding[HEADER_FILE_PADDING_LENGTH];
    uint check_sum;
    std::string to_str() const{
        std::string str = std::string("n_index_pages: ") + std::to_string(n_index_pages) + std::string(", n_data_pages: ") + std::to_string(n_data_pages);
        return str;
    }
};

const int LOG_HEADER_FILE_PADDING_LENGTH = PAGE_SIZE - sizeof(uint) - CHECKSUM_SIZE;
struct LogHeaderFilePage{
    uint n_log_pages;
    char padding[LOG_HEADER_FILE_PADDING_LENGTH];
    uint check_sum;
    std::string to_str() const{
        std::string str = std::string("n_log_pages: ") + std::to_string(n_log_pages);
        return str;
    }
};


struct IndexItem{
    char key[MAX_KEY_LENGTH]; //32
    uint last_page_ID;
    uint last_slot_ID;
    std::string to_str() const{
        std::string str = std::string("[key: ") + std::string(key) + std::string(", last_page_ID: ") + std::to_string(last_page_ID)
                            + std::string(", last_slot_ID: ") + std::to_string(last_slot_ID) + std::string("]");
        return str;
    }
};

const int INDEX_FILE_PADDING_LENGTH = PAGE_SIZE - N_INDEX_ITEMS_PER_PAGE * (sizeof(IndexItem)) - N_INDEX_ITEMS_PER_PAGE / 8;
struct IndexFilePage{
    IndexItem index_items[N_INDEX_ITEMS_PER_PAGE];
    char padding[INDEX_FILE_PADDING_LENGTH];
    char bitmap[N_INDEX_ITEMS_PER_PAGE / 8];
    std::string to_str() const{
        std::string str = "";
        for(uint i = 0; i < N_INDEX_ITEMS_PER_PAGE; i++){
            uint group_ID = i / 8;
            uint bit_ID = i % 8;
            unsigned char mask = 1 << bit_ID;
            if(bitmap[group_ID] & mask){
                str += std::string("slot ") + std::to_string(i) + std::string(": ") + index_items[i].to_str() + std::string("\n");
            }
        }
        return str;
    }
};


struct DataRecord{
    DataRecord(bool _first_slot_flag, bool _delete_flag, int _value, uint _prev_page_ID,
        uint _prev_slot_ID, uint _timestamp): first_slot_flag(_first_slot_flag), 
        delete_flag(_delete_flag), value(_value), prev_page_ID(_prev_page_ID), 
        prev_slot_ID(_prev_slot_ID), timestamp(_timestamp){}
    bool first_slot_flag;
    bool delete_flag;
    int value;
    uint prev_page_ID;
    uint prev_slot_ID;
    uint timestamp;
    std::string to_str() const{
        std::string str = std::string("[first_slot_flag: ") + std::to_string(first_slot_flag) + std::string(", delete_flag: ") + std::to_string(delete_flag)
                            + std::string(", value: ") + std::to_string(value) + std::string(", prev_page_ID: ") + std::to_string(prev_page_ID) + 
                            std::string(", prev_slot_ID: ") + std::to_string(prev_slot_ID) + std::string(", timestamp: ") + std::to_string(timestamp)
                            + std::string("]");
        return str;
    }
};

const int DATA_FILE_PAGE_SIZE = PAGE_SIZE - N_DATA_RECORDS_PER_PAGE * (sizeof(DataRecord)) - N_DATA_RECORDS_PER_PAGE / 8;
struct DataFilePage{
    DataRecord data_records[N_DATA_RECORDS_PER_PAGE];
    char padding[DATA_FILE_PAGE_SIZE];
    char bitmap[N_DATA_RECORDS_PER_PAGE / 8];
    std::string to_str() const{
        std::string str = "";
        for(uint i = 0; i < N_DATA_RECORDS_PER_PAGE; i++){
            uint group_ID = i / 8;
            uint bit_ID = i % 8;
            unsigned char mask = 1 << bit_ID;
            if(bitmap[group_ID] & mask){
                str += std::string("slot ") + std::to_string(i) + std::string(": ") + data_records[i].to_str() + std::string("\n");
            }
        }
        return str;
    }
};

enum LogType{
    DATA_RECORD_INSERT_LOG,
    DATA_RECORD_UPDATE_LOG,
    DATA_SLOT_LOG,
    INDEX_ITEM_INSERT_LOG,
    INDEX_ITEM_UPDATE_LOG,
    INDEX_SLOT_LOG,
    DATA_RECORD_UNDO_LOG,
    DATA_SLOT_UNDO_LOG,
    INDEX_ITEM_UNDO_LOG,
    INDEX_SLOT_UNDO_LOG,
    COMMIT_LOG,
    ABORT_LOG,
    CHECKPOINT_LOG    
};

struct LogItem{
    LogType log_type;
    uint transaction_ID;
    uint page_ID;
    uint offset;
    uint length;
    uint max_transaction_ID;
    uint max_timestamp;
    uint log_ID;
    uint undo_log_ID;
    char hash[HASH_LENGTH + 1];
    char old_value[sizeof(IndexItem)];
    char new_value[sizeof(IndexItem)];
};



const int LOG_PAGE_PADDING_SIZE = PAGE_SIZE - N_LOG_ITEMS_PER_PAGE * sizeof(LogItem) - N_LOG_ITEMS_PER_PAGE / 8 - CHECKSUM_SIZE;
struct LogFilePage{
    LogItem log_items[N_LOG_ITEMS_PER_PAGE];
    char paddding[LOG_PAGE_PADDING_SIZE];
    char bitmap[N_LOG_ITEMS_PER_PAGE / 8];
    unsigned int check_sum;    
};