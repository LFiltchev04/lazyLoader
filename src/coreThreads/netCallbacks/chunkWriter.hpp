
#pragma once
#include <cstdint>
#include <cstring>
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

        if(it->slabBuffer == nullptr){
            it->slabBuffer = bufPool->getSlab();
            it->slabBuffer->written = 0;
        }
        slab* slb = it->slabBuffer;

        
        memcpy(slb->data + slb->written, data, len);
        slb->written += len;

        if(!(flags & NGHTTP2_FLAG_END_STREAM)){
            return 0;
        }
        
        pointOffset poffset = chunkDecode(slb->data, slb->written);
        if(slb->written < sizeof(pointOffset) + poffset.chunkSize){
            return -1;
        }

        auto sqe = io_uring_get_sqe(&ring);
        if(sqe == nullptr){
            return -1;
        }
        io_uring_sqe_set_data(sqe, it);

        printf("poffset is offsetPtr: %zu, chunkSize: %zu\n", (size_t)poffset.offsetPtr, (size_t)poffset.chunkSize);
        io_uring_prep_write(sqe, it->fileFd, slb->data + sizeof(pointOffset), poffset.chunkSize, poffset.offsetPtr);
        it->targetWrite = poffset.chunkSize;
        it->filePosPointer += poffset.chunkSize;
        it->openOps++;

        io_uring_submit(&ring);

        return 0;
    }