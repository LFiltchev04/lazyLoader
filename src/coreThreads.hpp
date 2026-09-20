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
#include <sys/eventfd.h>
#include <unordered_set>
#include <mutex>
#include <optional>
#include <string>

#include "blobState.hpp"
#include "databaseSingleton.hpp"
#include "smallfilesBuffer.hpp"
#include "largefilesBuffer.hpp"

//ive got waaaay too much redundant security checks revisit to drop them, this is the hot path for file loads after all


struct fileOp {
    std::string filePath;
    uint32_t filePosPointer;
    int fanotifyFd;
    int fileDesc;
    int openOps = 0;
};



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
inline std::optional<std::string> resolveFdPath(int fd) {
    char linkPath[32];
    char filePath[PATH_MAX];
    std::snprintf(linkPath, sizeof(linkPath), "/proc/self/fd/%d", fd);
    ssize_t pathLen = readlink(linkPath, filePath, sizeof(filePath) - 1);
    if (pathLen < 0) {
        return std::nullopt;
    }
    //construct from length directly, readlink does not null-terminate and this avoids the extra strlen
    return std::string(filePath, static_cast<size_t>(pathLen));
}

//
void wipeFilePullState(const uint32_t filePullId){
    std::lock_guard<std::mutex> guard(activeFilePullsStateLock);
    
    auto it = activeFilePullsGlobal->find(filePullId);
    if(it != activeFilePullsGlobal->end()){
        

        if(it->second.openOps == 0){
            eventfd_t chanMsg = reinterpret_cast<eventfd_t>(&it->second.fanotifyFd);
            //important to note the actual hashmap wipe happens in the event handler, this is just a helper for emitting events to chan
            eventfd_write(fanotifyToNetwork, chanMsg);
            activeFilePullsGlobal->erase(it);
        }
    }
    
}



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





void fileAccessEventLoop(databaseSingleton* dbSingleton){
    std::unordered_set<std::string> trackedFiles;
    fanotifyToNetwork = eventfd(50, 0);

    
    int fanFd = fanotify_init(FAN_CLASS_PRE_CONTENT | FAN_CLOEXEC, O_RDONLY | O_LARGEFILE);
    fanotifyGlobalFd = fanFd;

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
    epoll_ctl(epFd, EPOLL_CTL_ADD, uringToFanotifyGlobal, &events[0]);
    
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
            if (events[i].data.fd == uringToFanotifyGlobal){
                                
                unblockOp.response = FAN_ALLOW;
                unblockOp.fd = 
                                write(fanFd, &unblockOp, sizeof(unblockOp));


                continue;
            }

            char buf[PATH_MAX];
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

                auto filePath = resolveFdPath(meta->fd);
                if (filePath) {
                    trackedFiles.insert(*filePath);
                    
                    //have to do this entire thing to ensure that i am getting the reference that wont die after it goes out of scope
                    auto it = trackedFiles.find(*filePath);
                    eventfd_t chanMsg = reinterpret_cast<eventfd_t>(std::addressof(it));
                    eventfd_write(fanotifyToNetwork ,meta->fd);
                    
                    std::cout << "catch all: " << *filePath << "\n";
                } else {
                    // Real file pulling for pre-examined lists happens here
                    
                    
                    std::cout << "OPEN: <unknown>\n";


                }

                close(meta->fd);
            }
        }
    }



}






void networkEventLoop() {
   int eventChanFd = eventfd(0, EFD_NONBLOCK);
   
    //figure out how to use IORING_SETUP_SQPOLL
    struct io_uring ring;
    if(io_uring_queue_init(64,&ring,0) < 0){
        std::cout << "Liburing ring init failed" << std::endl;
        return;
    }

    globalRing = ring;

    //this is the hashmap for disk uploads
    std::unordered_map<int32_t, fileOp> activeFilePulls;
    activeFilePullsGlobal = &activeFilePulls;

    struct parseCallbackCtx{
        std::unordered_map<int32_t, fileOp> *activeFilePulls;
        struct io_uring *ring;
        int eventChanFd;
    } globalCBctx = {&activeFilePulls, &ring, eventChanFd};
   
    
    nghttp2_session_callbacks* callbacks;
    nghttp2_session_callbacks_new(&callbacks);
    nghttp2_session* session;
    nghttp2_session_client_new(&session, callbacks, &globalCBctx);
    

    nghttp2_session_callbacks_set_on_frame_recv_callback(callbacks, [](nghttp2_session *session, const nghttp2_frame *frame, void *user_data) -> int {
        std::lock_guard<std::mutex> guard(activeFilePullsStateLock);
        std::cout << "on_frame_recv_callback: " << frame->hd.type << "\n";

        //ugly cast but eh
        std::unordered_map<int32_t, fileOp>* activePulls = static_cast<parseCallbackCtx*>(user_data)->activeFilePulls;

        if(frame->headers.cat == NGHTTP2_HCAT_RESPONSE){
            //this is where the 200 OK ought to result in a state machine reservation for an incoming download request
            std::cout << "server side 200 on file pull" << std::endl;

            if(activePulls->find(frame->hd.stream_id) == activePulls->end()){
                frame->hd.stream_id;
                activePulls->insert({frame->hd.stream_id, {0u, 0}});    
            }
            
        }

        if(frame->headers.cat == NGHTTP2_FLAG_END_STREAM){
            //dont know what to do with this
            std::cout << "server side end of stream on file pull" << std::endl;
            //fileOp *fo = &activePulls->find(frame->hd.stream_id)->second;
            
            //if(fo->fileDesc != 0){
            //    close(fo->fileDesc);
            //}
            
            //activePulls->erase(frame->hd.stream_id);
        }
        
        return 0;
    });


    nghttp2_session_callbacks_set_on_data_chunk_recv_callback(callbacks, [](nghttp2_session *session, uint8_t flags, int32_t stream_id, const uint8_t *data, size_t len, void *user_data) -> int {
        //could maybe manage thread contention better but it will have to work for now, lots of bugs possible if i decide to manually lock/unlock the thing
        std::lock_guard<std::mutex> guard(activeFilePullsStateLock);
        std::cout << "on_data_chunk_recv_callback: " << len << "\n";
        auto& activeFilePulls = *static_cast<parseCallbackCtx*>(user_data)->activeFilePulls;
        auto& ring = *static_cast<parseCallbackCtx*>(user_data)->ring;
        auto& eventChanFd = static_cast<parseCallbackCtx*>(user_data)->eventChanFd;
        
        
        auto it = activeFilePulls.find(stream_id);
        if(it != activeFilePulls.end()){

            std::cout << "server side data chunk on file pull" << std::endl;

            
            //data is stupid, its basically a pointer to a byte, i need to iterate the pointer until i get to size_t len to actually read everyting, it looks horridly dangeroud
        
            if(it->second.fileDesc == 0) {
                it->second.filePath;
                // should unpack the layer from relative path but will work for now, maybe server side
                it->second.fileDesc = open(it->second.filePath.c_str(), O_WRONLY, 0644);
                if (it->second.fileDesc < 0) {
                    std::perror("open");
                    return -1;
                }
            }
        
            for(size_t x = 0; x < len; x++){
                auto sqe = io_uring_get_sqe(&ring);
                io_uring_sqe_set_data(sqe, &it->second);
                
                //part of a potential IORING_SETUP_SQPOLL implementation 
                //if(*(&ring)->sq.kflags & IORING_SQ_NEED_WAKEUP){
                //    io_uring_sqe_set_flags(sqe, IORING_ENTER_SQ_WAKEUP);
                //}

                //theese can be submitted 
                io_uring_prep_write(sqe, it->second.fileDesc, &data[x], len, it->second.filePosPointer);
                io_uring_submit(&ring);
                it->second.filePosPointer += len;
                it->second.openOps++;
                
                io_uring_cqe* cqe;


                //sometimes theese get inlined and complete fast so i need to check whether that was the case 
                //to quikly kill the fanotify watcher and hashmap context
                while(io_uring_peek_cqe(&ring, &cqe) == 0){
                    cqe->res;
                    std::cout << "cqe res: " << cqe->res << std::endl;
                    auto state = reinterpret_cast<fileOp*>(cqe->user_data);
                    
                    
                    state->filePath;
                    //potential place to cleanup or log the file operation state
                    io_uring_cqe_seen(&ring, cqe);

                    it->second.openOps--;

                    if(it->second.openOps == 0){
                        close(it->second.fileDesc);

                        activeFilePulls.erase(it);
                    }
                }

                uint64_t minValue;
                

                //this is blocking, ideally avoid it because all request processing will die until disk commit
                //write(it->second.fileDesc, &data[x], 1);
            }
            

        }
        
        //cleanup logic to dump out the other state on fail of server pull for later
        return 0;
    });

    bufferPool* bPool = new bufferPool(100, 16);

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8080);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    int client_fd = socket(AF_INET, SOCK_STREAM, 0);
    

    connect(client_fd, (struct sockaddr*)&addr, sizeof(addr));

    nghttp2_nv hdrs[] = {
        MAKE_NV(":method", "GET"),
        MAKE_NV(":path", "/index.html"),
        MAKE_NV(":scheme", "http"),
        MAKE_NV("user-agent", "nghttp2/1.0")
    };



    int epollfd = epoll_create1(0);
    epoll_ctl(epollfd, EPOLL_CTL_ADD, client_fd, nullptr);
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fanotifyToNetwork, nullptr);
    epoll_event event;

    while(true){
        int n = epoll_wait(epollfd, &event, 1, -1);
        if(n < 0){
            std::cout << "epoll_wait failed" << std::endl;
            return;
        }




        //alternate control path for doing initial registration of file pulls with path
        if(event.data.fd == fanotifyToNetwork){
            eventfd_t chanMsg;
            eventfd_read(fanotifyToNetwork, &chanMsg);
            auto it = reinterpret_cast<int*>(chanMsg);
            auto filePathOpt = resolveFdPath(*it);

            if(filePathOpt){
                std::cout << "file path: " << *filePathOpt << std::endl;
            } else {
                std::cout << "file path could not be resolved for fd: " << *it << std::endl;
                throw std::runtime_error("file path could not be resolved for fd: " + std::to_string(*it));
            }

            int32_t streamId = nghttp2_submit_request(session, nullptr, hdrs, sizeof(hdrs), nullptr, nullptr);
            
            fileOp newOp(
                filePathOpt.value(), //file path
                0, //file pos pointer
                *it, //fanotify file descriptor
                0, // liburing file descriptor for populating content
                0 //ongoing liburing writes semaphore
            );

            activeFilePulls.insert({streamId, newOp});
        }



        for(int x = 0; x < 4; x++){
            auto slab = bPool->getSlab();
            ssize_t readNum = read(client_fd, slab->data, sizeof(slab->data));
            if(readNum <= 0){
                break;
            }

            std::cout << "read " << readNum << " bytes\n";
            nghttp2_session_mem_recv(session, reinterpret_cast<const uint8_t*>(slab->data), readNum);
        
        }
        
    }

}




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

