#include "genericStatAlloc.hpp"



template <typename T>
staticAllocatedPool<T>::staticAllocatedPool(int maxSize) {
    this->maxSize = maxSize;
    stackPointer = &pool[0];

}

template <typename T>
T* staticAllocatedPool<T>::get() {
    std::lock_guard<std::mutex> lock(poolLock);

    if(stackPointer == &pool[maxSize]) {
        printf("static allocation pool exhausted for type: %s\n", typeid(T).name());
        return nullptr;
    }

    return stackPointer++;
}

template <typename T>
void staticAllocatedPool<T>::yield(T* item) {
    std::lock_guard<std::mutex> lock(poolLock);
    if((item >= &pool[maxSize]) or (item < &pool)){
        printf("Invalid item passed to yield, ignoring\n");
        return;
    }
}

//internal compaction function for references
template <typename T>
void staticAllocatedPool<T>::refPushdown() {
    

    T* ptrAdv = &pool[0];
    T* ptrBackmark = &pool[0];
    
    for(int i = 0; i < maxSize; i++){
        ptrAdv += 1;

        if(ptrBackmark != nullptr and ptrAdv != nullptr){
            ptrBackmark += 1;
            ptrAdv += 1;
        }


        if(ptrBackmark == nullptr){
            //stays there
            ptrAdv += 1;
            if(ptrAdv != nullptr){
                //that ought to bubble holes right out of it
                ptrBackmark = ptrAdv;
                ptrAdv = nullptr;

                ptrBackmark += 1;
                ptrAdv += 1;
            }
        }

    }
}