#include <staydb/util/file_util.h>
#include <sys/stat.h>
#include <cstring>
#include <cerrno>
#include <cassert>
#include <cstdio>


void mkdir_recursive(const char *dir) {
    char tmp[256];
    char *p = NULL;
    size_t len;

    snprintf(tmp, sizeof(tmp),"%s",dir);
    len = strlen(tmp);
    if(tmp[len - 1] == '/')
        tmp[len - 1] = 0;
    for(p = tmp + 1; *p; p++){
        if(*p == '/') {
            *p = 0;
            if(mkdir(tmp, S_IRWXU) == -1){
                assert(errno == EEXIST);
            }
            *p = '/';
        }
    }
    if(mkdir(tmp, S_IRWXU) == -1){
        assert(errno == EEXIST);
    }
}

void create_file(const char *name) {
    FILE *file = fopen(name, "a+");
    assert(file);
    fclose(file);
}