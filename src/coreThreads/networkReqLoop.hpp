#include <nghttp2/nghttp2.h>
#include <liburing.h>
#include <iostream>
#include <unordered_map>
#include <fcntl.h>
#include <vector>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/epoll.h>


#include "../smallfilesBuffer.hpp"
#include "../utils/streamDecodeWrite.hpp"
#include "fileOp.hpp"


struct ngStaticContext{
    bufferPool *bPool;
    io_uring* ring;
    int eventChanFd;
    int* serverFd;
};

void networkEventLoop(io_uring* ring, int *eventChanFd, std::string serverEndp) {
   //gotta wipe theese

    
    //figure out how to use IORING_SETUP_SQPOLL
    struct io_uring ring;
    if(io_uring_queue_init(8192, ring, 0) < 0){
        std::cout << "Liburing ring init failed" << std::endl;
        return;
    }

    bufferPool* bPool = new bufferPool(100, 16);

    //this is the hashmap for disk uploads
    std::unordered_map<int32_t, fileOp> activeFilePulls;

    
    ngStaticContext globalCBctx = {bPool, ring, *eventChanFd, nullptr};
   
    
    nghttp2_session_callbacks* callbacks;
    nghttp2_session_callbacks_new(&callbacks);
    nghttp2_session* session;
    nghttp2_session_client_new(&session, callbacks, &globalCBctx);
    

    nghttp2_session_callbacks_set_on_frame_recv_callback(callbacks, );
    nghttp2_session_callbacks_set_on_header_recv_callback(callbacks, );
    nghttp2_session_callbacks_set_on_data_chunk_recv_callback(callbacks, );

    //dont know what to use this for, might have to start thinking about it when i decide to do the server
    




    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8080);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    *serverFd = socket(AF_INET, SOCK_STREAM, 0);
    

    connect(serverFd, (struct sockaddr*)&addr, sizeof(addr));

    



    int epollfd = epoll_create1(0);
    epoll_ctl(epollfd, EPOLL_CTL_ADD, serverFd, nullptr);
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fanotifyToNetwork, nullptr);
    epoll_event event;

    //static buffer that gets stuff memcp-ed out of to internal nghttp2 stuff 
    char buffer[65536];

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
            
            /*
            fileOp newOp(
                filePathOpt.value(), //file path
                0, //file pos pointer
                *it, //fanotify file descriptor
                0, // liburing file descriptor for populating content
                0 //ongoing liburing writes semaphore
            );
            */

            //activeFilePulls.insert({streamId, newOp});
        }



        for(int x = 0; x < 4; x++){
            ssize_t readNum = read(serverFd, buffer, sizeof(buffer));
            if(readNum <= 0){
                break;
            }

            //will rely on the internal buffers to memcpy, slabs will be pushed to callback management so i can accuratley track offsets
            std::cout << "read " << readNum << " bytes\n";
            nghttp2_session_mem_recv(session, reinterpret_cast<const uint8_t*>(buffer), readNum);
        
        }
        
    }

}
