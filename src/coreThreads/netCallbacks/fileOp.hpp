#include <vector>
#include <cstdint>
#include <string>

#include "../../smallfilesBuffer.hpp"


struct fileOp {
    std::vector<slab*> frameBuffer; //imma get rid of this garbage 
    
    std::string filePath;
    uint32_t filePosPointer;
    int fanotifyFd;
    int networkFd;
    int openOps = 0;
};
