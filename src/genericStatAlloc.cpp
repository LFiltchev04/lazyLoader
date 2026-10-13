#include "genericStatAlloc.hpp"



template <typename T, uint32_t maxSize>
staticAllocatedPool<T, maxSize>::staticAllocatedPool() {
    for(int i = 0; i < maxSize; i++){
        poolRef.push(&pool[i]);
    }

}

template <typename T, uint32_t maxSize>
void staticAllocatedPool<T, maxSize>::get(T* destination) {
    std::lock_guard<std::mutex> lock(poolLock);

    if(poolRef.empty()){
        destination = nullptr;
        return;
    }
    destination = poolRef.top();
    poolRef.pop();
}

template <typename T, uint32_t maxSize>
void staticAllocatedPool<T, maxSize>::yield(T* item) {
    //dont need to lock that, its a static pool range, it wont change
    if((item >= &pool[maxSize]) or (item < &pool)){
        printf("Invalid item passed to yield, ignoring\n");
        return;
    }

    std::lock_guard<std::mutex> lock(poolLock);
    poolRef.push(item);
}
