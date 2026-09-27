#include <stack>
#include <mutex>

#include "smallfilesBuffer.hpp"

struct writeCtx {
    slab* targetSlab;
    int fanotifyFd; //file descriptor to unblock
    int fileDescriptor; //the write location for the uring handler
    int targetWrites; //number of completions needed to finish the operation
    int filePointer; 
    int fileOffset; 
};

template <typename T>
class staticAllocatedPool{
    std::mutex poolLock;
    uint32_t maxSize;
    uint32_t currentSize;
    T pool[maxSize];

    std::stack<T*> poolRef;
    

    public:
    staticAllocatedPool(int maxSize);

    void get(T*);
    void yield(T* item);

};