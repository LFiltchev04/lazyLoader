#pragma once
#include <nghttp2/nghttp2.h>
#include <iostream>
#include <unordered_map>
#include "parseCallbackCtx.hpp"

#include "../fileOp.hpp"
#include "parseCallbackCtx.hpp"


int frameRecvCb(nghttp2_session *session, const nghttp2_frame *frame, void *user_data){

    std::cout << "on_frame_recv_callback: " << static_cast<int>(frame->hd.type) << "\n";

    if(frame->hd.type == NGHTTP2_HEADERS && frame->headers.cat == NGHTTP2_HCAT_RESPONSE){
        std::cout << "server side 200 on file pull" << std::endl;
    }

    if(frame->hd.type == NGHTTP2_DATA){
        std::cout << "       server side data frame on file pull" << std::endl;
    }

    if((frame->hd.type == NGHTTP2_HEADERS || frame->hd.type == NGHTTP2_DATA) && (frame->hd.flags & NGHTTP2_FLAG_END_STREAM)){
        std::cout << "server side end of stream on file pull" << std::endl;
    }

    return 0;
}