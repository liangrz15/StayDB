#pragma once
#include "const.h"

const int HEADER_FILE_PADDING_LENGTH = PAGE_SIZE - 2 * sizeof(unsigned int) - CHECKSUM_SIZE;
struct HeaderFilePage{
    uint n_index_pages;
    uint n_data_pages;
    char padding[HEADER_FILE_PADDING_LENGTH];
    uint check_sum;
};


struct IndexItem{
    char key[MAX_KEY_LENGTH]; //32
    uint page_ID;
    uint lastSlot_ID;
};

const int INDEX_FILE_PADDING_LENGTH = PAGE_SIZE - N_INDEX_ITEMS_PER_PAGE * (sizeof(IndexItem)) - N_INDEX_ITEMS_PER_PAGE / 8;
struct IndexFilePage{
    IndexItem index_items[N_INDEX_ITEMS_PER_PAGE];
    char padding[INDEX_FILE_PADDING_LENGTH];
    char bitmap[N_INDEX_ITEMS_PER_PAGE / 8];
};


struct DataRecord{
    bool first_slot_flag;
    bool delete_flag;
    int value;
    uint prev_page_ID;
    uint prev_slot_ID;
    unsigned long long timestamp;
};

const int DATA_FILE_PAGE_SIZE = PAGE_SIZE - N_DATA_RECORDS_PER_PAGE * (sizeof(DataRecord)) - N_DATA_RECORDS_PER_PAGE / 8;
struct DataFilePage{
    DataRecord data_records[N_DATA_RECORDS_PER_PAGE];
    char padding[DATA_FILE_PAGE_SIZE];
    char bitmap[N_DATA_RECORDS_PER_PAGE / 8];
};

enum LogType{
    DataRecordLog,
    DataSlotLog,
    IndexItemLog,
    IndexSlotLog,
    DataRecordUndoLog,
    DataSlotUndoLog,
    IndexItemUndoLog,
    IndexSlotUndoLog,
    CommitLog,
    AbortLog,
    CheckpointLog    
};

struct LogItem{
    LogType log_type;
    uint transaction_ID;
    char key[MAX_KEY_LENGTH]; //32
    uint page_ID;
    uint offset;
    uint length;
    uint largest_transaction_ID;
    uint log_ID;
    uint undo_log_ID;
    char old_value[sizeof(DataRecord)];
    char new_value[sizeof(DataRecord)];
};



const int LOG_PAGE_PADDING_SIZE = PAGE_SIZE - N_LOG_ITEMS_PER_PAGE * sizeof(LogItem) - N_LOG_ITEMS_PER_PAGE / 8 - CHECKSUM_SIZE;
struct LogFilePage{
    LogItem log_items[N_LOG_ITEMS_PER_PAGE];
    char paddding[LOG_PAGE_PADDING_SIZE];
    char bitmap[N_LOG_ITEMS_PER_PAGE / 8];
    unsigned int check_sum;    
};