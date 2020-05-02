#pragma once
#include<string>


/* executor for read/write/insert/delete command */
class Executor{

public:
    int read_key(const std::string& key, int* value);
    int write_key(const std::string& key, int value);
    int insert_key(const std::string& key, int value);
    int delete_key(const std::string& key, int value);
    int abort();
    int commit();
};