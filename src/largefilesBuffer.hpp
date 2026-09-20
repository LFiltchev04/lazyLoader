#include <stack>
#include <cstdint>
#define LARGE_SLAB_POOL_COUNT 32



//pretty sure this thing is padding data and definitley larger than a single page but meh
struct bigSlab {
    char slab[65535]; // single contiguous buffer per slab
    uint32_t currentOffset; // tracks the current offset for appending data
};

//this is used to avoid too harsh of an overhead with allocating many completion queues for liburing. 
class largeAppendBuffer{
    bigSlab* slabPool; // contiguous slab pool head pointer
    std::stack<bigSlab*> slabStack;

    public:
    largeAppendBuffer();
    bigSlab* initAppendSlab();
    //may trigger a flush to disk depending on size, is append only
    void appendBuffer(bigSlab* slab, void* data, size_t len);
    void freeSlab(bigSlab* slab);
    
};