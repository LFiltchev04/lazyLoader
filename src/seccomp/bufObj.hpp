#pragma once
#include <string>
#include "containerClass.hpp"


struct shmPage{
    int memcpLocation;
    uint32_t len;
    std::string filename;
    int ctrPipeFd;
    container* ctrDb;
    bool pipeWriter(void*data, size_t size);
};
