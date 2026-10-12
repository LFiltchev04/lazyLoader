#include <cstdint>
#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <liburing.h>
#include <nghttp2/nghttp2.h>


#include "parseCallbackCtx.hpp"



int dataChunkWriter(nghttp2_session *session, 
        uint8_t flags, int32_t stream_id, 
        const uint8_t *data, size_t len, 
        void *user_data){
        //could maybe manage thread contention better but it will have to work for now, lots of bugs possible if i decide to manually lock/unlock the thing
        std::cout << "on_data_chunk_recv_callback: " << len << "\n";
        auto& activeFilePulls = *static_cast<parseCallbackCtx*>(user_data)->activeFilePulls;
        auto& ring = *static_cast<parseCallbackCtx*>(user_data)->ring;
        auto& eventChanFd = static_cast<parseCallbackCtx*>(user_data)->eventChanFd;
        
        
        auto it = activeFilePulls.find(stream_id);
        if(it != activeFilePulls.end()){

            std::cout << "server side data chunk on file pull" << std::endl;

            
            //data is stupid, its basically a pointer to a byte, i need to iterate the pointer until i get to size_t len to actually read everyting, it looks horridly dangerous
        
            if(it->second.fileDesc == 0) {
                it->second.filePath;
                // should unpack the layer from relative path but will work for now, maybe server side
                it->second.fileDesc = open(it->second.filePath.c_str(), O_WRONLY, 0644);
                if (it->second.fileDesc < 0) {
                    return -1;
                }
            }
        
            for(size_t x = 0; x < len; x++){
                auto sqe = io_uring_get_sqe(&ring);
                io_uring_sqe_set_data(sqe, &it->second);
                
                //part of a potential IORING_SETUP_SQPOLL implementation 
                //if(*(&ring)->sq.kflags & IORING_SQ_NEED_WAKEUP){
                //    io_uring_sqe_set_flags(sqe, IORING_ENTER_SQ_WAKEUP);
                //}

                //theese can be submitted 
                io_uring_prep_write(sqe, it->second.fileDesc, &data[x], len, it->second.filePosPointer);
                it->second.filePosPointer += len;
                it->second.openOps++;
                
                io_uring_submit(&ring);

                io_uring_cqe* cqe;


                //sometimes theese get inlined and complete fast so i need to check whether that was the case 
                //to quikly kill the fanotify watcher and hashmap context
                while(io_uring_peek_cqe(&ring, &cqe) == 0){
                    cqe->res;
                    std::cout << "cqe res: " << cqe->res << std::endl;
                    auto state = reinterpret_cast<fileOp*>(cqe->user_data);
                    
                    
                    state->filePath;
                    //potential place to cleanup or log the file operation state
                    io_uring_cqe_seen(&ring, cqe);

                    it->second.openOps--;

                    if(it->second.openOps == 0){
                        close(it->second.fileDesc);

                        activeFilePulls.erase(it);
                    }
                }

                uint64_t minValue;
                

                //this is blocking, ideally avoid it because all request processing will die until disk commit
                //write(it->second.fileDesc, &data[x], 1);
            }
            

        }
        
        //cleanup logic to dump out the other state on fail of server pull for later
        return 0;
    }