#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <string>
#include <unordered_map>
#include <cstdint>

#include "containerClass.hpp"
#include "../bufObj.hpp"



class mmapIndexer{
    std::unordered_map<std::string, shmPage> pageMap; //maps container name to page buffer
    
    public:
    mmapIndexer();
    
    void addActive(const std::string& containerName, int ctrPipeFd);
    bool checkActive(const std::string& containerName);
    void removeActive(const std::string& containerName);
    shmPage* getPage(const std::string& containerName);

};