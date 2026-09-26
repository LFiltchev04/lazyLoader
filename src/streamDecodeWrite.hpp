//is used exclusivley for decoding partial uploads asynchronously


#include "filePacker.hpp"
#include "smallfilesBuffer.hpp"




void chunkDecode(uint8_t* dataPtr, ssize_t chunkSize){
    uint16_t filePathLen = *((uint16_t*)dataPtr);
    uint16_t fileSize = *((uint16_t*)(dataPtr + sizeof(uint16_t)));
    std::string filePath((char*)(dataPtr + 2 * sizeof(uint16_t)), filePathLen);

    // pass to context the write information

   
}