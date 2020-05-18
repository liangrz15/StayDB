#include <staydb/util/prepare.h>
#include <fstream>
#include <cassert>

void wrap_transaction(const char* origin_file, const char* wrapped_file){
    std::ifstream in_stream(origin_file);
    std::ofstream out_stream(wrapped_file);
    if(!in_stream){
        assert(0);
    }
    std::string command_type;
    std::string key;
    int value;
    std::string first_operand;
    std::string op;
    int second_operand;
    out_stream << "BEGIN 1000000000\n"; 
    while(true){
        in_stream >> command_type;
        if(in_stream.fail()){
            break;
        }
        if(command_type == "BEGIN"){
            assert(0);
        }
        else if(command_type == "INSERT"){
            in_stream >> key >> value;
            out_stream << "INSERT " << key << " " << value << "\n";
        }
        else if(command_type == "READ"){
            in_stream >> key;
            out_stream << "INSERT " << key << "\n";
        }
        else if(command_type == "SET"){
            in_stream >> key >> first_operand >> op >> second_operand;
            key = key.substr(0, key.length() - 1); // get rid of the ","
            out_stream << "SET " << key << ", " << first_operand << " " << op << " " << second_operand << "\n";
        }
        else if(command_type == "COMMIT"){
            assert(0);
        }
    }
    out_stream << "COMMIT 1000000000\n"; 
}