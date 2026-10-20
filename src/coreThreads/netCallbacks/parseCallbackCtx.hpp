#pragma once
#include <unordered_map>
#include <netinet/in.h>
#include "../../smallfilesBuffer.hpp"
#include <nghttp2/nghttp2.h>

struct parseCallbackCtx{
        bufferPool* bPool;
        struct io_uring *ring;
        int eventChanFd;
        int tcpClientFd;
        sockaddr_in addr;
        nghttp2_session* session;
        parseCallbackCtx() : bPool(nullptr), ring(nullptr), eventChanFd(-1), tcpClientFd(-1), session(nullptr) {}
};