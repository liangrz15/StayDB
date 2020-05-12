#pragma once
#include <string>

typedef unsigned int uint;


const int PAGE_SIZE = 4096;
const int CHECKSUM_SIZE = sizeof(uint);

const int MAX_KEY_LENGTH = 32;
const int N_INDEX_ITEMS_PER_PAGE = 96;


const int N_DATA_RECORDS_PER_PAGE = 160;
const int N_LOG_ITEMS_PER_PAGE = 32;

const std::string HEADER_TYPE = "header";
const std::string INDEX_TYPE = "index";
const std::string DATA_TYPE = "data";
const std::string BACKUP_TYPE = "backup";



