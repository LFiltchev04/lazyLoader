
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
#include "../../smallfilesBuffer.hpp"



int dataChunkWriter(nghttp2_session *session, 
        uint8_t flags, int32_t stream_id, 
        const uint8_t *data, size_t len, 
        void *user_data){
        //could maybe manage thread contention better but it will have to work for now, lots of bugs possible if i decide to manually lock/unlock the thing
            printf("\n\n\n DATA WRITER \n\n\n");

        auto& ring = *static_cast<parseCallbackCtx*>(user_data)->ring;
        auto& eventChanFd = static_cast<parseCallbackCtx*>(user_data)->eventChanFd;
        auto& bufPool = static_cast<parseCallbackCtx*>(user_data)->bPool;
        
        printf("data writer is up?\n");
        
        auto t = nghttp2_session_get_stream_user_data(session, stream_id);

        auto it = static_cast<fileOp*>(t);
        std::cout << "server side data chunk on file pull" << std::endl;

        it->bPool = bufPool;
        
        if(it->fileFd < 0) {
                
            // should unpack the layer from relative path but will work for now, maybe server side
            it->fileFd = open(it->filePath.c_str(), O_WRONLY, 0644);
            if (it->fileFd < 0) {
                return -1;
            }
        }
        
            
        auto sqe = io_uring_get_sqe(&ring);
        io_uring_sqe_set_data(sqe, it);
        
        
        

        uint8_t* constCast = const_cast<uint8_t*>(data);
        pointOffset poffset = chunkDecode(constCast, len);

        printf("poffset is offsetPtr: %zu, chunkSize: %zu\n", poffset.offsetPtr, poffset.chunkSize);
        //theese can be submitted, since the data coming over is sequential and properly sized its easy to just put it in there
        
        slab* slb;
        
        if(it->slabBuffer == nullptr){
            slb = bufPool->getSlab();
            slb->written = len;
            it->slabBuffer = slb;
        }else{
            slb = it->slabBuffer;
        }
        

        if(!(flags & NGHTTP2_FLAG_END_STREAM)){
            printf("END_STREAM flag not received\n");
            memcpy(it->slabBuffer->data, data, len);
            return 0;
        }
        

        
        io_uring_prep_write(sqe, it->fileFd, slb->data + slb->trim, poffset.chunkSize, poffset.offsetPtr);
        it->filePosPointer += poffset.chunkSize;
        it->openOps++;

        printf("submitting uring\n");
        io_uring_submit(&ring);
        
        //cleanup logic to dump out the other state on fail of server pull for later
        return 0;
    }