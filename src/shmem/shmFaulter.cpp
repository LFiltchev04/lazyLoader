#include "shmFaulter.hpp"

mmapIndexer::mmapIndexer() {
}

void mmapIndexer::addActive(const std::string& containerName, int ctrPipeFd) {
    shmPage page;
    page.ctrPipeFd = ctrPipeFd;
    pageMap[containerName] = page;
}

bool mmapIndexer::checkActive(const std::string& containerName) {
    shmPage* page = getPage(containerName);
    if(page == nullptr){
        return false;
    }
    
    uint8_t ctrSignal = 0; //just checks if its running
    if(page->pipeWriter(&ctrSignal, sizeof(ctrSignal)) == false){
        return false;
    }

    return true;
}

void mmapIndexer::removeActive(const std::string& containerName) {
    if(pageMap.find(containerName) != pageMap.end()){
        pageMap.erase(containerName);
    }
}

shmPage* mmapIndexer::getPage(const std::string& containerName) {
    if(pageMap.find(containerName) != pageMap.end()){
        return &pageMap[containerName];
    }
    throw std::runtime_error("Container not found in pageMap");
}