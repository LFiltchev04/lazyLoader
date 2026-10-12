#include <nghttp2/nghttp2.h>
#include <iostream>
#include <unordered_map>
#include "parseCallbackCtx.hpp"

#include "fileOp.hpp"
#include "parseCallbackCtx.hpp"


int frameRecvCb(nghttp2_session *session, const nghttp2_frame *frame, void *user_data){
        std::cout << "on_frame_recv_callback: " << frame->hd.type << "\n";

        //ugly cast but eh

        fileOp *ctx = new fileOp();
        auto tCast = static_cast<void*>(ctx);
        nghttp2_session_set_stream_user_data(session, frame->headers.hd.stream_id, tCast);

        if(frame->headers.cat == NGHTTP2_HCAT_RESPONSE){
            //this is where the 200 OK ought to result in a state machine reservation for an incoming download request
            std::cout << "server side 200 on file pull" << std::endl;

            
            
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
    }