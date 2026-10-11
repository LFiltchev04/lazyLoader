#include "fileAccessLoop.hpp"

void fileAccessEventLoop(blobState *blobDb, std::string *basePath, int *fanotifyToNetwork, int *uringToFanotify){
    std::unordered_set<std::string> trackedFiles;

    
    int fanFd = fanotify_init(FAN_CLASS_PRE_CONTENT | FAN_CLOEXEC, O_RDONLY | O_LARGEFILE);

    if(basePath != nullptr){
        //used primarily for unit tests, the blobDb object carries the data alone no problem
        struct stat strStat;
        stat(basePath->c_str(), &strStat);

        if(S_ISDIR(strStat.st_mode)){
            markTree(basePath->c_str(), fanFd);
        }
    }

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
    epoll_ctl(epFd, EPOLL_CTL_ADD, *uringToFanotify, &events[0]);
    
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
            if (events[i].data.fd == *uringToFanotify){
                
                uringCompletionEmit *emitData;
                read(*uringToFanotify, emitData, sizeof(*emitData));

                unblockOp.response = FAN_ALLOW;
                unblockOp.fd = emitData->accFd;
                write(fanFd, &unblockOp, sizeof(unblockOp));

                //i am annoyed by the allocations and syscalls
                //want to push it all to the mmaped DB ideally, that ought to resolve faster than this garbage
                std::string pthResolv = resolveFdPath(emitData->accFd).value();

                
                if(emitData->writtenBytes){
                    if(blobDb != nullptr){
                        fileState fStat = blobDb->getFileState(pthResolv);
                    
                        //preliminary assumption the file was pulled through completely, easy to change relativley
                        blobDb->updateFileState(pthResolv, fileState::PRESENT);
                    }else{
                        //this is a temp workaround to get the test without a full db init
                    }
                    

                    //thats gonna blow up innit
                    fanotify_mark(fanFd, FAN_MARK_REMOVE, FAN_ALL_EVENTS | FAN_ALL_PERM_EVENTS, AT_FDCWD, pthResolv.c_str());
                }

                //all cases ought to get rid of this
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
                    //first have to remove the global path prefix, ignored for testing

                    trackedFiles.insert(*filePath);
                    
                    //have to do this entire thing to ensure that i am getting the reference that wont die after it goes out of scope
                    auto it = trackedFiles.find(*filePath);

                    //for the network thread to clean up
                    fanotifyNetworkInit *pullInit = new fanotifyNetworkInit;
                    pullInit->fPath = *filePath;
                    pullInit->fanPermFd = meta->fd;
                    
                    write(*fanotifyToNetwork ,pullInit, sizeof(*pullInit));
                    
                }

            }
        }
    }
}