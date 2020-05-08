#pragma once
#include "filelayout.h"
#include <vector>

class LogManager{
public:
    static LogManager* get_instance();
    LogItem add_log(LogType log_type, uint transaction_ID, char key[MAX_KEY_LENGTH], 
                uint page_ID, uint offset, uint length, char* old_value, char* new_value);
    void redo_logs(std::vector<LogItem> log_items);
    void recover();

private:
    LogManager* instance;
};