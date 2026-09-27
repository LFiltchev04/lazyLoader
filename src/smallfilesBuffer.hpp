#include <stack>
#include <cstdint>

struct slab{
    uint8_t data[16384];
    uint16_t written = 0;
    uint8_t trim = 0;
};


//singleton that creates a buffer pool for incoming file chunks over http2, needed for async writes
class bufferPool {
    std::stack<slab*> freeSlabs;
    unsigned int bufferCounts;
    unsigned int minSlabCount;
    bool dynAllocate = false;

    void dynamicAllocate();

    public:
    bufferPool(unsigned int bufferCounts, unsigned int minSlabCount = 20);
    bool checkFull();
    slab* getSlab();
    void freeSlab(slab* slab);
    bool poolExhausted();

};