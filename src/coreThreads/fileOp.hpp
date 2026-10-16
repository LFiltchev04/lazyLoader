#pragma once
#include <cstdint>
#include <string>

#include "../smallfilesBuffer.hpp"


struct fileOp {
    
    std::string filePath;
    uint32_t filePosPointer = 0;
    uint16_t targetWrite = 0;
    bufferPool* bPool = nullptr;
    slab* slabBuffer = nullptr;

    int fanotifyFd;
    int networkFd;
    int fileFd = -1;

    int openOps = 0;
};
