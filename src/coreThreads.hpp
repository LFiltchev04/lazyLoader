#include <iostream>
#include <thread>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <sys/fanotify.h>
#include <fcntl.h>
#include <liburing.h>
#include <nghttp2/nghttp2.h>
#include <unordered_set>
#include <mutex>
#include <optional>
#include <string>

#include "blobState.hpp"
#include "databaseSingleton.hpp"
#include "smallfilesBuffer.hpp"
#include "filePacker.hpp"
#include "streamDecodeWrite.hpp"

//ive got waaaay too much redundant security checks revisit to drop them, this is the hot path for file loads after all





//need theese globals for cross-thread state machine updates, its a stupid thing to do in the first place but meh
int fanotifyGlobalFd = -1;
int fanotifyToNetwork = -1;
int uringToFanotifyGlobal = -1;
int pullReqCompl = -1;
io_uring globalRing;
std::unordered_map<int32_t, fileOp>* activeFilePullsGlobal;
std::mutex activeFilePullsStateLock;



#define MAKE_NV(NAME, VALUE) \
    { (uint8_t *)(NAME), (uint8_t *)(VALUE), sizeof(NAME) - 1, sizeof(VALUE) - 1, NGHTTP2_NV_FLAG_NONE }


//goes to proc to resolve the file name, is very convenient because the fanotify file descriptor passes through the eventfd channels without having to keep separate lookups
















































//i do not yet know whether i want to turn this into its separate thread or just integrate it in the fd loop, will see.
void uringCompletionHandler() {
    int epfd = epoll_create1(0);    

    epoll_event event;
    event.events = EPOLLIN;
    int epInitCode = epoll_ctl(epfd, EPOLL_CTL_ADD, globalRing.ring_fd, &event);
    if(epInitCode < 0){
        std::cout << "epoll_ctl for uring handler failed" << std::endl;
        return;
    }

    io_uring_cqe* cqe;
    while(true){
        std::lock_guard<std::mutex> guard(activeFilePullsStateLock);

        int n = epoll_wait(epfd, &event, 1, -1);
        if(n < 0){
            std::cout << "epoll_wait failed for uring handler" << std::endl;
            return;
        }

        unsigned head;
        unsigned int c = 0u;
        io_uring_for_each_cqe(&globalRing, head, cqe){
            uint8_t cBackStatus = io_uring_peek_cqe(&globalRing, &cqe);
            if(cBackStatus != 0){
                std::cout << "io_uring_peek_cqe failed with status: " << static_cast<int>(cBackStatus) << std::endl;
            }

            uint32_t filePullId = static_cast<uint32_t>(cqe->user_data);
            auto it = activeFilePullsGlobal->find(filePullId);
            if(it != activeFilePullsGlobal->end()){
                wipeFilePullState(filePullId);
            }
            

            c++;
        }

    }
}

//will have to revisit this



//databaseSingleton* dbSingleton = new databaseSingleton();
//std::thread pullRequestThread(pullRequestInit, dbSingleton);

