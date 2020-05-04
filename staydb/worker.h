#pragma once

#include<string>


/* The thread worker that parses the command and manage the thread transaction. */
class Worker{

public:
    void execute_file(std::string input_file_name, std::string output_file_name);
};