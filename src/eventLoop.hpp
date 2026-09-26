#include <sys/epoll.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/fanotify.h>
#include <fcntl.h>
#include <iostream>


#include "blobState.hpp"
#include "databaseSingleton.hpp"
#include "addMarks.hpp"
//#include "httplib.h

int startEventLoop(int queueFd, databaseSingleton* dbSingleton){
    epoll_ctl(queueFd, EPOLL_CTL_ADD, queueFd, nullptr);

    while(true){
        epoll_event event;
        int n = epoll_wait(queueFd, &event, 1, -1);
        if(n < 0){
            return -1;
        }

        char messageIn[39];
        ssize_t status = recv(queueFd, &messageIn, sizeof(messageIn), 0);
        if(status < 0){
            return -1;
        }

        uint8_t lazyLoadable = static_cast<uint8_t>(messageIn[0]);
        if (lazyLoadable == 1u) {
            char tempStr[36];
            std::memcpy(tempStr, messageIn + 3, 36);
            dbSingleton->registerNewBlob(messageIn);
            
        }
    }

    return 0;

}



void fileAccessEventLoop(databaseSingleton* dbSingleton){
    int fanFd = fanotify_init(FAN_CLASS_CONTENT, O_RDONLY | O_LARGEFILE | O_NONBLOCK);
    if(fanFd < 0){
        throw new std::runtime_error("fanotify_init failed");
    }

    int epFd = epoll_create1(0);
    if(epFd < 0){
        throw new std::runtime_error("epoll_create1 failed");
    }
    
    //this is the main loop of the file fetcher service, the individual file requests are fired off in this execution path
    while (true) {
        epoll_event events[1];
        int n = epoll_wait(epFd, events, 1, -1);
        std::cout << "unblocked" <<std::endl;
        if (n < 0) {
            if (errno == EINTR) continue;
            std::perror("epoll_wait");
            break;
        }

        for (int i = 0; i < n; ++i) {
            if (events[i].data.fd != fanFd) continue;

            char buf[4096];
            ssize_t len = read(fanFd, buf, sizeof(buf));
            if (len <= 0) {
                if (errno == EINTR) continue;
                std::perror("read fanotify");
                continue;
            }

            for (fanotify_event_metadata* meta = reinterpret_cast<fanotify_event_metadata*>(buf);FAN_EVENT_OK(meta, static_cast<int>(len));meta = FAN_EVENT_NEXT(meta, len)) {
                std::cout << "event meta is: " << meta->mask <<std::endl;

                //this is questionable, if it starts throwing version errors ill revisit
                //if (meta->vers != FANOTIFY_METADATA_VERSION) {
                //    std::cerr << "fanotify metadata version mismatch\n";
                //    continue;
                //}

                if (meta->fd < 0) continue;

                char linkPath[64];
                char filePath[PATH_MAX];

                std::snprintf(linkPath, sizeof(linkPath), "/proc/self/fd/%d", meta->fd);
                ssize_t pathLen = readlink(linkPath, filePath, sizeof(filePath) - 1);
                
                if (pathLen >= 0) {
                    filePath[pathLen] = '\0';
                    
                    
                    std::cout << "catch all: " << filePath << "\n";
                } else {
                    // Real file pulling for pre-examined lists happens here

                    //this has to be a coroutine, but for now just a dummy function call to figure out the underlying code

                    
                    
                    std::cout << "OPEN: <unknown>\n";


                }

                close(meta->fd);
            }
        }
    }



}


