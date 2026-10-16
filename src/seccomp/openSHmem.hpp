#include <sys/mman.h>
#include <string>
#include <fcntl.h>
#include <unistd.h>

#define SHMEM_SIZE 4096 //for now, one page only


void* openSHmem(std::string name){
    int fd = shm_open(name.c_str(), O_RDWR, 0666);
    if(fd == -1){
        return nullptr;
    }
    void* addr = mmap(nullptr, SHMEM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(addr == MAP_FAILED){
        return nullptr;
    }
    return addr;
}