#include "fileAccessLoop.hpp"

void fileAccessEventLoop(blobState &blobDb, int fanotifyToNetwork, int uringToFanotify){
    std::unordered_set<std::string> trackedFiles;
    fanotifyToNetwork = eventfd(50, 0);

    
    int fanFd = fanotify_init(FAN_CLASS_PRE_CONTENT | FAN_CLOEXEC, O_RDONLY | O_LARGEFILE);


    if(fanFd < 0){
        throw new std::runtime_error("fanotify_init failed");
    }

    int epFd = epoll_create1(0);
    if(epFd < 0){
        throw new std::runtime_error("epoll_create1 failed");
    }

    //unblock logic for fanotify events
    struct fanotify_response unblockOp;
    
    //main network loop fd
    epoll_event events[1];
    epoll_ctl(epFd, EPOLL_CTL_ADD, fanFd, &events[0]);
    epoll_ctl(epFd, EPOLL_CTL_ADD, uringToFanotify, &events[0]);
    
    //this ensures that i must run the fileAccessLoop first. 
    
    //this is the main loop of the file fetcher service, the individual file requests are fired off in this execution path

    while (true) {
        int n = epoll_wait(epFd, events, 1, -1);
        std::cout << "unblocked" <<std::endl;
        if (n < 0) {
            if (errno == EINTR) continue;
            std::perror("epoll_wait");
            break;
        }

        
        

        for (int i = 0; i < n; ++i) {
            if (events[i].data.fd == uringToFanotify){
                
                uringCompletionEmit *emitData;
                read(uringToFanotify, emitData, sizeof(*emitData));

                unblockOp.response = FAN_ALLOW;
                unblockOp.fd = emitData->accFd;
                write(fanFd, &unblockOp, sizeof(unblockOp));

                if(emitData->writtenBytes){
                    fileState fStat = blobDb.getFileState(resolveFdPath(emitData->accFd).value());
                    blobDb.
                }

                delete emitData;
                
                continue;
            }

            char buf[PATH_MAX];
            ssize_t len = read(fanFd, buf, sizeof(buf));
            if (len <= 0) {
                if (errno == EINTR) continue;
                std::perror("read fanotify");
                continue;
            }

            for (fanotify_event_metadata* meta = reinterpret_cast<fanotify_event_metadata*>(buf); FAN_EVENT_OK(meta, static_cast<int>(len)); meta = FAN_EVENT_NEXT(meta, len)) {
                std::cout << "event meta is: " << meta->mask <<std::endl;

                //this is questionable, if it starts throwing version errors ill revisit
                //if (meta->vers != FANOTIFY_METADATA_VERSION) {
                //    std::cerr << "fanotify metadata version mismatch\n";
                //    continue;
                //}

                

                if (meta->fd < 0) continue;

                std::optional<std::string> filePath = resolveFdPath(meta->fd);
                if (filePath) {
                    //first have to remove the global path prefix

                    trackedFiles.insert(*filePath);
                    
                    //have to do this entire thing to ensure that i am getting the reference that wont die after it goes out of scope
                    auto it = trackedFiles.find(*filePath);
                    eventfd_t chanMsg = reinterpret_cast<eventfd_t>(std::addressof(it));
                    eventfd_write(fanotifyToNetwork ,meta->fd);
                    
                    std::cout << "catch all: " << *filePath << "\n";
                    
                }

            }
        }
    }
}