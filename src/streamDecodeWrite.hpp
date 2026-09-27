//is used exclusivley for decoding partial uploads asynchronously


#include "filePacker.hpp"
#include "smallfilesBuffer.hpp"


struct pointOffset{
    uint64_t offsetPtr; //the offset within the file where this chunk should be written
    uint16_t fileSize; //the actual chunk size length, so sub-buffer files dont break
    uint16_t trimLength = 0; //the whole header length so that i can remove it easily later
};

//returns metadata on precise write location plus advances the data pointer past the metadata header block
pointOffset chunkDecode(uint8_t* dataPtr, ssize_t chunkSize){
    uint64_t offsetPtr = *((uint64_t*)dataPtr);
    uint16_t fileSize = *((uint16_t*)(dataPtr + sizeof(uint64_t) + sizeof(uint16_t)));
    
    //advances pointer past the metadata header block
    uint16_t trimLength = 0;
    trimLength += sizeof(uint64_t);
    trimLength += sizeof(uint16_t);
    trimLength += sizeof(uint16_t);


    // pass to context the write information
    pointOffset result{offsetPtr, fileSize, trimLength};
    return result;
}