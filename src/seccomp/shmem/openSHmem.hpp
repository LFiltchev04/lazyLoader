#include <sys/mman.h>
#include <string>
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <cstring>

#include "../bufObj.hpp"
#define SHMEM_SIZE 4096 //for now, one page only


shmPage openSHmem(std::string name){
    
    shmPage pg;

    std::string completePath = "/tmp/shmSignals/" + name;
    mkdir("/tmp/shmSignals", 0777);

    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    if(completePath.size() >= sizeof(addr.sun_path)){
        return shmPage{};
    }
    strncpy(addr.sun_path, completePath.c_str(), sizeof(addr.sun_path) - 1);

    int sock = socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    if(sock == -1){
        return shmPage{};
    }
    unlink(completePath.c_str());
    if(bind(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == -1){
        close(sock);
        return shmPage{};
    }
    pg.ctrPipeFd = sock;

    pg.memcpLocation = shm_open(name.c_str(), O_RDWR, 0666);
    if(pg.memcpLocation == -1){
        close(sock);
        unlink(completePath.c_str());
        return shmPage{};
    }

    return pg;
}