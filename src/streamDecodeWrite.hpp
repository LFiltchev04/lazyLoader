//is used exclusivley for decoding partial uploads asynchronously


#include "filePacker.hpp"
#include "smallfilesBuffer.hpp"


struct pointOffset{
    uint16_t filePathLen; //to be able to snip out the path anme properly
    uint16_t fileSize; //the actual chunk size length, so sub-buffer files dont break
    uint16_t trimLength = 0; //the whole header length so that i can remove it easily later
    std::string filePath; // the path name, pretty sure i dont need this as hot path uploads are tagged by per-file stream ids anyhow.
};

//returns metadata on precise write location plus advances the data pointer past the metadata header block
pointOffset chunkDecode(uint8_t* dataPtr, ssize_t chunkSize){
    uint16_t filePathLen = *((uint16_t*)dataPtr);
    uint16_t fileSize = *((uint16_t*)(dataPtr + sizeof(uint16_t)));
    std::string filePath((char*)(dataPtr + 2 * sizeof(uint16_t)), filePathLen);

    //advances pointer past the metadata header block
    uint16_t trimLength = 0;
    trimLength += sizeof(uint16_t);
    trimLength += sizeof(uint16_t);
    trimLength += filePathLen;
    


    // pass to context the write information
    pointOffset result{filePathLen, fileSize, trimLength, filePath};
    return result;
}