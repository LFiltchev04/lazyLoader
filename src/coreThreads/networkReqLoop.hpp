#pragma once
#include <nghttp2/nghttp2.h>
#include <liburing.h>
#include <iostream>
#include <fcntl.h>
#include <vector>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <mutex>
#include <cerrno>
#include <unistd.h>
#include <sys/eventfd.h>


#include "../smallfilesBuffer.hpp"
#include "fileOp.hpp"
#include "netCallbacks/requestResource.hpp"
#include "fanotifyNetworkInit.hpp"
#include "netCallbacks/parseCallbackCtx.hpp"
#include "netCallbacks/frameRecv.hpp"
#include "netCallbacks/headerRecv.hpp"
#include "netCallbacks/chunkWriter.hpp"


//ensures contexts are set and there is a connected socket and session with callbacks
parseCallbackCtx* idempSockConnect(parseCallbackCtx* connCtx, int epfd, const sockaddr_in& serverAddr, io_uring* ring, bufferPool* bPool){
    if(connCtx != nullptr){
        return connCtx;
    }else{
        connCtx = new parseCallbackCtx{};
        connCtx->addr = serverAddr;
        connCtx->ring = ring;
        connCtx->bPool = bPool;
    }


    connCtx->tcpClientFd = socket(AF_INET, SOCK_STREAM, 0);
    if(connCtx->tcpClientFd < 0){
        std::cout << "Failed to create socket" << std::endl;
        connCtx->tcpClientFd = -1;
        return connCtx;
    }

    if(connect(connCtx->tcpClientFd, (sockaddr*)&connCtx->addr, sizeof(connCtx->addr)) < 0){
        perror("Failed to connect to server");
        close(connCtx->tcpClientFd);
        connCtx->tcpClientFd = -1;
        return connCtx;
    }

    
    printf("Connected to server on fd %d\n", connCtx->tcpClientFd);
    epoll_event eventExtern{};
    eventExtern.events = EPOLLIN;
    eventExtern.data.ptr = connCtx;
    epoll_ctl(epfd, EPOLL_CTL_ADD, connCtx->tcpClientFd, &eventExtern);
    
    //inits a session and submits settings headers

    nghttp2_session_callbacks* callbacks;
    nghttp2_session_callbacks_new(&callbacks);
    nghttp2_session_callbacks_set_on_frame_recv_callback(callbacks, frameRecvCb);
    nghttp2_session_callbacks_set_on_header_callback(callbacks, headerCb);
    nghttp2_session_callbacks_set_on_data_chunk_recv_callback(callbacks, dataChunkWriter);

    //one session per connection, the per request state lives in the stream user data

    
    nghttp2_session_client_new(&connCtx->session, callbacks, connCtx);
    //connection preface + SETTINGS
    nghttp2_settings_entry entr[2];
    entr[0].settings_id = NGHTTP2_SETTINGS_MAX_CONCURRENT_STREAMS;
    entr[0].value = 100;
    entr[1].settings_id = NGHTTP2_SETTINGS_INITIAL_WINDOW_SIZE;
    entr[1].value = 65535;
    nghttp2_submit_settings(connCtx->session, NGHTTP2_FLAG_NONE, entr, 2);
    
    return connCtx;
}



void networkEventLoop(io_uring* ring, int fanotifyToNetwork, std::string serverEndp, bufferPool* bPool) {

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8080);
    inet_pton(AF_INET, serverEndp.c_str(), &addr.sin_addr);


    int epollfd = epoll_create1(0);

    epoll_event fanEvent{}; //fanotify input event declaration
    fanEvent.events = EPOLLIN;
    fanEvent.data.fd = fanotifyToNetwork;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fanotifyToNetwork, &fanEvent);

    nghttp2_session_callbacks* callbacks;
    nghttp2_session_callbacks_new(&callbacks);
    nghttp2_session_callbacks_set_on_frame_recv_callback(callbacks, frameRecvCb);
    nghttp2_session_callbacks_set_on_header_callback(callbacks, headerCb);
    nghttp2_session_callbacks_set_on_data_chunk_recv_callback(callbacks, dataChunkWriter);

    //one session per connection, the per request state lives in the stream user data
    nghttp2_session* session = nullptr;


    epoll_event uEvent;
    uEvent.events = EPOLLIN;

    int evfd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    io_uring_register_eventfd(ring, evfd);
    uEvent.data.fd = evfd;

    int epInitCode = epoll_ctl(epollfd, EPOLL_CTL_ADD, evfd, &uEvent);
    if(epInitCode < 0){
        std::cout << "epoll_ctl for uring handler failed" << std::endl;
        return;
    }

    io_uring_cqe* cqe;
    uint64_t eventfdBlackHole;
    epoll_event evs[2];


    //static buffer that gets stuff memcp-ed out of to internal nghttp2 stuff
    char buffer[65536];

    while(true){

        
        int n = epoll_wait(epollfd, evs, 2, -1);

        if(n < 0){
            if(errno == EINTR){
                continue;
            } 
            std::cout << "epoll_wait failed" << std::endl;
            return;
        }

        for(int i = 0; i < n; i++){

        if(evs[i].data.fd == fanotifyToNetwork){ //gates to the flow that starts a file download from fanotify
            fanotifyNetworkInit *i = nullptr;
            if(read(fanotifyToNetwork, &i, sizeof(i)) != sizeof(i) || i == nullptr){
                continue;
            }

            
            auto connCtx = idempSockConnect(nullptr, epollfd, addr, ring, bPool);
            connCtx->eventChanFd = i->fanPermFd;
            

            std::cout << "Received fanotify network init " << i->fPath << std::endl;

            sendHeaders(connCtx->session, std::move(i->fPath), connCtx->tcpClientFd, i->fanPermFd);
            while(nghttp2_session_want_write(connCtx->session)){
                flushSession(connCtx->session, connCtx->tcpClientFd);
            }
                    
            delete i; //here too

        }else if(evs[i].data.fd == evfd){ //Gates to the uring handler flow, if it will even continue to exist

            printf("completion arrived\n");
            unsigned head;
            unsigned int c = 0u;
            read(evs[i].data.fd, &eventfdBlackHole, 8);

            io_uring_for_each_cqe(ring, head, cqe){

                printf("cqe drain event\n");
                auto* ctx = static_cast<fileOp*>(io_uring_cqe_get_data(cqe));
                if(cqe->res <= 0){
                    std::cout << "io_uring_peek_cqe failed with status: " << static_cast<int>(cqe->res) << std::endl;
                }

                //again, i am assuming it wont short at all so i leave it as is. 
                ctx->openOps--;
                if(ctx->targetWrite != cqe->res){
                
                    std::cout << "\n\n\n\nMismatch between openOps and cqe->res: " << ctx->targetWrite << " vs " << cqe->res << "\n\n\n\n";
                }            
                //returns the allocated memory to the buf pool
                ctx->bPool->freeSlab(ctx->slabBuffer);
                ctx->slabBuffer = nullptr;
                ctx->slabBuffer = nullptr;
                c++;
            }

            io_uring_cq_advance(ring, c);
        }else{ //Gates to the data parse and input flow, all the bulk bytes flow in here
            
            auto* connCtx = static_cast<parseCallbackCtx*>(evs[i].data.ptr);
            
                
            ssize_t readNum = read(connCtx->tcpClientFd, buffer, sizeof(buffer));
            printf("read %zd bytes\n", readNum);
                

            if(readNum <= 0){
                break;
            }

            //WRITE PROCESS 

            std::cout << "read " << readNum << " bytes\n";

            if(nghttp2_session_want_write(connCtx->session)){ //if it needs to write it relies on EPOLLOUT to keep it running
                struct epoll_event e;
                e.events = EPOLLIN | EPOLLOUT;
                e.data.ptr = connCtx;
                epoll_ctl(epollfd, EPOLL_CTL_MOD, connCtx->tcpClientFd, &e);
                flushSession(connCtx->session, connCtx->tcpClientFd);
            }else{ //has completed write, enters wait state
                struct epoll_event e;
                e.events = EPOLLIN;
                e.data.ptr = connCtx;
                epoll_ctl(epollfd, EPOLL_CTL_MOD, connCtx->tcpClientFd, &e);
            }

             //READ PROCESS

            ssize_t used = nghttp2_session_mem_recv(connCtx->session, reinterpret_cast<const uint8_t*>(buffer), readNum);
            printf("consumed %zd bytes\n", used);
                
            }
            
           
        
    }
}
}
