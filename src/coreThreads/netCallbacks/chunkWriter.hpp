
#pragma once
#include <cstdint>
#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <liburing.h>
#include <nghttp2/nghttp2.h>


#include "parseCallbackCtx.hpp"
#include "../fileOp.hpp"
#include "../../utils/streamDecodeWrite.hpp"



int dataChunkWriter(nghttp2_session *session, 
        uint8_t flags, int32_t stream_id, 
        const uint8_t *data, size_t len, 
        void *user_data){
        //could maybe manage thread contention better but it will have to work for now, lots of bugs possible if i decide to manually lock/unlock the thing
            printf("\n\n\n DATA WRITER \n\n\n");

        auto& ring = *static_cast<parseCallbackCtx*>(user_data)->ring;
        auto& eventChanFd = static_cast<parseCallbackCtx*>(user_data)->eventChanFd;
        
        printf("data writer is up?\n");
        
        auto t = nghttp2_session_get_stream_user_data(session, stream_id);

        auto it = static_cast<fileOp*>(t);
            std::cout << "server side data chunk on file pull" << std::endl;

            
            //data is stupid, its basically a pointer to a byte, i need to iterate the pointer until i get to size_t len to actually read everyting, it looks horridly dangerous
        
            if(it->fileFd < 0) {
                
                // should unpack the layer from relative path but will work for now, maybe server side
                it->fileFd = open(it->filePath.c_str(), O_WRONLY, 0644);
                if (it->fileFd < 0) {
                    return -1;
                }
            }
        
            pointOffset poffset;
            poffset.chunkSize = 600;
            poffset.offsetPtr = 0;
            for(size_t x = 0; x < len-sizeof(pointOffset);){
                auto sqe = io_uring_get_sqe(&ring);
                io_uring_sqe_set_data(sqe, it);
                
                //part of a potential IORING_SETUP_SQPOLL implementation 
                //if(*(&ring)->sq.kflags & IORING_SQ_NEED_WAKEUP){
                //    io_uring_sqe_set_flags(sqe, IORING_ENTER_SQ_WAKEUP);
                //}

                uint8_t* constCast = const_cast<uint8_t*>(&data[x]);
                //pointOffset poffset = chunkDecode(constCast, len);

                printf("submitting uring\n");
                //theese can be submitted, since the data coming over is sequential and properly sized its easy to just put it in there
                io_uring_prep_write(sqe, it->fileFd, &constCast[x], 1024, 0);
                it->filePosPointer += poffset.chunkSize;
                it->openOps++;
                
                x += poffset.chunkSize;
            }
            
            io_uring_submit(&ring);
            
        
        //cleanup logic to dump out the other state on fail of server pull for later
        return 0;
    }