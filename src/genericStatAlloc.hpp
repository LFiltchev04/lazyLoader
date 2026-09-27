#include <stack>
#include <mutex>

#include "smallfilesBuffer.hpp"

struct writeCtx {
    slab* targetSlab;
    int fanotifyFd; //file descriptor to unblock
    int targetWrites; //number of completions needed to finish the operation
    int filePointer;
    int fileOffset; 
    int fileDescriptor;
};

template <typename T>
class staticAllocatedPool{
    std::mutex poolLock;
    uint32_t maxSize;
    
    T pool[maxSize];
    T* stackRef[maxSize];
    
    
    T* stackPointer;
    void refPushdown();

    public:
    staticAllocatedPool(int maxSize);

    T* get();
    void yield(T* item);

};