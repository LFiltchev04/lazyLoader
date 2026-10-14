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


#include "../smallfilesBuffer.hpp"
#include "fileOp.hpp"
#include "netCallbacks/requestResource.hpp"
#include "fanotifyNetworkInit.hpp"
#include "netCallbacks/parseCallbackCtx.hpp"
#include "netCallbacks/frameRecv.hpp"
#include "netCallbacks/headerRecv.hpp"
#include "netCallbacks/chunkWriter.hpp"


//makes sure there is a socket connected, first init is harder than the following ones
bool idempSockConnect(int& sock, int& epfd, sockaddr_in& addr){
    if(sock != -1){
        return true;
    }

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if(sock < 0){
        std::cout << "Failed to create socket" << std::endl;
        sock = -1;
        return false;
    }

    if(connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0){
        perror("Failed to connect to server");
        close(sock);
        sock = -1;
        return false;
    }

    //nonblocking so the drain loop in the event loop can't stall on an empty socket
    fcntl(sock, F_SETFL, fcntl(sock, F_GETFL, 0) | O_NONBLOCK);

    printf("Connected to server on fd %d\n", sock);
    epoll_event eventExtern{};
    eventExtern.events = EPOLLIN;
    eventExtern.data.fd = sock;
    epoll_ctl(epfd, EPOLL_CTL_ADD, sock, &eventExtern);
    return true;
}



void networkEventLoop(io_uring* ring, int fanotifyToNetwork, std::string serverEndp, bufferPool* bPool, std::mutex& ringLock) {

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(7070);
    inet_pton(AF_INET, serverEndp.c_str(), &addr.sin_addr);

    int serverSock = -1;

    int epollfd = epoll_create1(0);

    epoll_event event{};
    event.events = EPOLLIN;
    event.data.fd = fanotifyToNetwork;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fanotifyToNetwork, &event);

    //callbacks are copied into the session when it is created, so they have to be fully set up before nghttp2_session_client_new
    nghttp2_session_callbacks* callbacks;
    nghttp2_session_callbacks_new(&callbacks);
    nghttp2_session_callbacks_set_on_frame_recv_callback(callbacks, frameRecvCb);
    nghttp2_session_callbacks_set_on_header_callback(callbacks, headerCb);
    nghttp2_session_callbacks_set_on_data_chunk_recv_callback(callbacks, dataChunkWriter);

    //one session per connection, the per request state lives in the stream user data
    nghttp2_session* session = nullptr;
    parseCallbackCtx *connCtx = new parseCallbackCtx{bPool, ring, -1, &serverSock};

    //static buffer that gets stuff memcp-ed out of to internal nghttp2 stuff
    char buffer[65536];

    while(true){
        int n = epoll_wait(epollfd, &event, 1, -1);
        if(n < 0){
            if(errno == EINTR) continue;
            std::cout << "epoll_wait failed" << std::endl;
            return;
        }

        if(event.data.fd == fanotifyToNetwork){
            fanotifyNetworkInit *i = nullptr;
            if(read(fanotifyToNetwork, &i, sizeof(i)) != sizeof(i) || i == nullptr){
                continue;
            }

            if(!idempSockConnect(serverSock, epollfd, addr)){
                delete i;
                continue;
            }

            if(session == nullptr){
                nghttp2_session_client_new(&session, callbacks, connCtx);
                //connection preface + SETTINGS
                nghttp2_submit_settings(session, NGHTTP2_FLAG_NONE, nullptr, 0);
            }

            std::cout << "Received fanotify network init " << i->fPath << std::endl;
            connCtx->eventChanFd = i->fanPermFd;
            sendHeaders(session, std::move(i->fPath), serverSock, connCtx);
            delete i;
        }else{
            ringLock.lock();
            while(true){
                ssize_t readNum = read(serverSock, buffer, sizeof(buffer));
                
                if(readNum <= 0){
                    if(readNum == 0){
                        std::cout << "server closed connection" << std::endl;
                        epoll_ctl(epollfd, EPOLL_CTL_DEL, serverSock, nullptr);
                        close(serverSock);
                        serverSock = -1;
                        nghttp2_session_del(session);
                        session = nullptr;
                    }
                    break;
                }
                std::cout << "read " << readNum << " bytes\n";
                ssize_t used = nghttp2_session_mem_recv(session, reinterpret_cast<const uint8_t*>(buffer), readNum);
                
            }
            
            ringLock.unlock();
        }
    }
}
