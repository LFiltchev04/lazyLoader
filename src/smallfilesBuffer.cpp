#include "smallfilesBuffer.hpp"



//stack allocates the buffer pools
bufferPool::bufferPool(unsigned int bufferCounts, unsigned int minSlabCount){
    
    if(minSlabCount == 0){
        //add errors later
        throw std::exception();    
    }
    this->minSlabCount = minSlabCount;
    
    if(bufferCounts == 0){
        //add errors later
        throw std::exception();    
    }

    this->bufferCounts = bufferCounts;
    for(unsigned int i = 0; i < bufferCounts; i++){
        freeSlabs.push(new slab());
    }

}

void bufferPool::dynamicAllocate(){
    if(freeSlabs.size() == 0){
        for(int x = 0; x < 20; x++){
            freeSlabs.push(new slab());
        }
    }
}

slab* bufferPool::getSlab(){
    if(freeSlabs.empty()){
        if(dynAllocate){
            dynamicAllocate();
        }

        throw std::exception();
    }


    slab* slab = freeSlabs.top();
    freeSlabs.pop();
    
    return slab;
}


void bufferPool::freeSlab(slab* slab){
    freeSlabs.push(slab);
}

bool bufferPool::poolExhausted(){

}