#include "containerPullInit.hpp"





//here the pre-allocation happens, it does lmdb calls to persist the state and registers it, is inited by the modified containerd
void pullRequestInit(databaseSingleton* dbSingleton) {
    //this thing ought to handle the incoming request for files from unix socket and initialize the 

    //i am making up this var here, its a placeholder for the unix socket that it ought to be set in some config
    std::string sockPath = "/tmp/unscrewLater.sock";
    int socFd = socket(AF_UNIX, SOCK_DGRAM, 0);

    int epfd = epoll_create1(0);
    epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = socFd;
    epoll_ctl(epfd, EPOLL_CTL_ADD, socFd, &ev);


    //running the assumption a single recv can drain it in one go, its dgram under minimums by almost any metric so i hope its ok?
    char messageIn[42];
    while (true) {
        epoll_wait(epfd, &ev, 1, -1);

        ssize_t status = recv(socFd, &messageIn, sizeof(messageIn), 0);
        if (status < 0) {
            // Handle error
            continue;
        }

        blobState newBlob(messageIn);

        if(dbSingleton->blobPresent(messageIn)){
            std::cout << "Attempted to register a blob already present in the DB" << std::endl;
            continue;
        }

        dbSingleton->registerNewBlob(messageIn);
        
        //this ought to tag the blob for fanotify after materialization and dial in the even loop for network pulls with the full path? 
        
    }
}


