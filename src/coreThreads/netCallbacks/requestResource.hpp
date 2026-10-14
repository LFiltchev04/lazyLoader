#pragma once
#include <nghttp2/nghttp2.h>
#include <string>
#include <unistd.h>
#include <fcntl.h>
#include <cerrno>
#include <cstdio>
#include "../fileOp.hpp"

#define MAKE_NV(NAME, VALUE) \
    { (uint8_t *)(NAME), (uint8_t *)(VALUE), sizeof(NAME) - 1, sizeof(VALUE) - 1, NGHTTP2_NV_FLAG_NONE }


bool flushSession(nghttp2_session* session, int sock){
    while(true){
        const uint8_t* data = nullptr;
        ssize_t len = nghttp2_session_mem_send(session, &data);
        if(len < 0) return false;
        if(len == 0) return true;

        ssize_t sent = 0;
        while(sent < len){
            ssize_t w = write(sock, data + sent, len - sent);
            if(w < 0){
                if(errno == EINTR || errno == EAGAIN) continue;
                return false;
            }
            sent += w;
        }
    }
}

void sendHeaders(nghttp2_session *session, std::string filePath, int outgoingFd, void* userDataCtx){

    printf("Sending headers for %s\n", filePath.c_str());
    
    nghttp2_nv nvArr[] = {
        MAKE_NV(":method", "GET"),
        MAKE_NV(":scheme", "http"),
        MAKE_NV(":authority", "localhost"),
        { (uint8_t *)":path", (uint8_t *)filePath.c_str(), sizeof(":path") - 1, filePath.size(), NGHTTP2_NV_FLAG_NONE }
    };

    fileOp *fop = new fileOp();
    fop->filePath = filePath;
    fop->fileFd = open(filePath.c_str(), O_RDWR);
    fop->filePosPointer = 0;
    fop->networkFd = outgoingFd;
    fop->fanotifyFd = -1;

    if(fop->fileFd < 0){
        printf("Failed to open file %s\n", filePath.c_str());
        delete fop;
        return;
    }

    nghttp2_priority_spec priSpec;
    nghttp2_priority_spec_init(&priSpec, 0, 256, 0);

    int32_t streamId = nghttp2_submit_request(session, &priSpec, nvArr, sizeof(nvArr) / sizeof(nvArr[0]), nullptr, fop);
    if(streamId < 0){
        close(fop->fileFd);
        delete fop;
        return;
    }

    flushSession(session, outgoingFd);
    printf("Finished sending headers for %s\n", filePath.c_str());
}