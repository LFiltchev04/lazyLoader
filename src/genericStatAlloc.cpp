#include "genericStatAlloc.hpp"



template <typename T>
staticAllocatedPool<T>::staticAllocatedPool(int maxSize) {
    this->maxSize = maxSize;
    for(int i = 0; i < maxSize; i++){
        poolRef.push(&pool[i]);
    }

}

template <typename T>
void staticAllocatedPool<T>::get(T* destination) {
    std::lock_guard<std::mutex> lock(poolLock);

    if(poolRef.empty()){
        destination = nullptr;
        return;
    }
    destination = poolRef.top();
    poolRef.pop();
}

template <typename T>
void staticAllocatedPool<T>::yield(T* item) {
    //dont need to lock that, its a static pool range, it wont change
    if((item >= &pool[maxSize]) or (item < &pool)){
        printf("Invalid item passed to yield, ignoring\n");
        return;
    }

    std::lock_guard<std::mutex> lock(poolLock);
    poolRef.push(item);
}

