#include "largefilesBuffer.hpp"
#include <stdexcept>
#include <cstring>


largeAppendBuffer::largeAppendBuffer(){
    this->slabPool = new bigSlab[LARGE_SLAB_POOL_COUNT];

    // Push in reverse so the first allocation returns the head slab.
    for (std::size_t i = LARGE_SLAB_POOL_COUNT; i > 0; --i) {
        slabStack.push(&slabPool[i - 1]);
    }
}

bigSlab* largeAppendBuffer::initAppendSlab(){
    if (slabStack.empty()) {
        throw std::runtime_error("No more largeSlabs available in the pool.");
    }

    bigSlab* slab = slabStack.top();
    slabStack.pop();
    return slab;
}

void largeAppendBuffer::appendBuffer(bigSlab *slab, void* data, size_t len){
    if (slab == nullptr) {
        throw std::invalid_argument("Slab pointer is null.");
    }

    if (len > sizeof(slab->slab) - slab->currentOffset) {
        throw std::overflow_error("Not enough space in the slab to append data.");
    }

    memcpy(slab->slab + slab->currentOffset, data, len);
    slab->currentOffset += len;
}

void largeAppendBuffer::freeSlab(bigSlab* slab){
    if (slab == nullptr) {
        return;
    }

    if (slab < slabPool || slab >= slabPool + LARGE_SLAB_POOL_COUNT) {
        throw std::invalid_argument("Attempting to free a slab that is not part of the pool.");
    }

    slabStack.push(slab);
}