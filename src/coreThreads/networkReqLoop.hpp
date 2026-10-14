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


#include "../smallfilesBuffer.hpp"
#include "fileOp.hpp"
#include "netCallbacks/requestResource.hpp"
#include "fanotifyNetworkInit.hpp"
#include "netCallbacks/parseCallbackCtx.hpp"
#include "netCallbacks/frameRecv.hpp"
#include "netCallbacks/headerRecv.hpp"
#include "netCallbacks/chunkWriter.hpp"


void networkEventLoop(io_uring* ring, int fanotifyToNetwork, std::string serverEndp, bufferPool* bPool, std::mutex& ringLock) {
   //gotta wipe theese

    
    //figure out how to use IORING_SETUP_SQPOLL
    if(io_uring_queue_init(8192, ring, 0) < 0){
        std::cout << "Liburing ring init failed" << std::endl;
        return;
    }

    //this is the hashmap for disk uploads

    
   
    
    nghttp2_session_callbacks* callbacks;
    nghttp2_session_callbacks_new(&callbacks);
    nghttp2_session* session;
    

    //dont know what to use this for, might have to start thinking about it when i decide to do the server
    


    

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(7070);
    inet_pton(AF_INET, serverEndp.c_str(), &addr.sin_addr);

    int serverSock = socket(AF_INET, SOCK_STREAM, 0);
    

    int connRes = connect(serverSock, (struct sockaddr*)&addr, sizeof(addr));
    if(connRes < 0){
        std::cout << "Failed to connect to server" << std::endl;
        return;
    }

    



    int epollfd = epoll_create1(0);
    epoll_ctl(epollfd, EPOLL_CTL_ADD, serverSock, nullptr);
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




        if(event.data.fd == fanotifyToNetwork){

            //will 100% break
            fanotifyNetworkInit *i;
            read(fanotifyToNetwork, &i, sizeof(fanotifyNetworkInit));
            delete i;
            
            //registers a stream ID and sync writes the small request header
            parseCallbackCtx *globalCBctx = new parseCallbackCtx{bPool, ring, i->fanPermFd, &serverSock};

            nghttp2_session_client_new(&session, callbacks, &globalCBctx);
    
            nghttp2_session_callbacks_set_on_frame_recv_callback(callbacks, frameRecvCb);
            nghttp2_session_callbacks_set_on_header_callback(callbacks, headerCb);
            nghttp2_session_callbacks_set_on_data_chunk_recv_callback(callbacks, dataChunkWriter);

            std::cout << "Received fanotify network init " << i->fPath << std::endl;
            sendHeaders(session, i->fPath, serverSock, globalCBctx);
        }


        ringLock.lock();
        //data is dumped here for callbacks to handle, uring completions take care of 
        for(int x = 0; x < 4; x++){
            ssize_t readNum = read(serverSock, buffer, sizeof(buffer));
            if(readNum <= 0){
                break;
            }
            //will rely on the internal buffers to memcpy, slabs will be pushed to callback management so i can accuratley track offsets
            std::cout << "read " << readNum << " bytes\n";
            nghttp2_session_mem_recv(session, reinterpret_cast<const uint8_t*>(buffer), readNum);
        
        }
        ringLock.unlock();
    }

}
