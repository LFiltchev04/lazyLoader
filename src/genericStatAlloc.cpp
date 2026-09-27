#include "genericStatAlloc.hpp"



template <typename T>
staticAllocatedPool<T>::staticAllocatedPool(int maxSize) {
    this->maxSize = maxSize;
    for(int i = 0; i < maxSize; i++){
        pool.push(new T());
    }

}

template <typename T>
T* staticAllocatedPool<T>::get() {
    std::lock_guard<std::mutex> lock(poolLock);

   

    if(pool.empty()){
        return nullptr;
    }
    T* item = pool.top();
    pool.pop();
    return item;
}

template <typename T>
void staticAllocatedPool<T>::yield(T* item) {
    std::lock_guard<std::mutex> lock(poolLock);
    pool.push(item);
}

