#pragma once
#include <cstdint>
#include <string>

#include "../smallfilesBuffer.hpp"


struct fileOp {
    
    std::string filePath;
    uint32_t filePosPointer = 0;
    uint16_t targetWrite = 0;
    bufferPool* bPool;
    slab* slabBuffer;

    int fanotifyFd;
    int networkFd;
    int fileFd;

    int openOps = 0;
};
