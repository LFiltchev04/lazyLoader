#include <nghttp2/nghttp2.h>
#include <liburing.h>
#include <iostream>
#include <unordered_map>
#include <fcntl.h>
#include <vector>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/epoll.h>


#include "../smallfilesBuffer.hpp"
#include "../utils/streamDecodeWrite.hpp"

struct fileOp {
    std::vector<slab*> frameBuffer;
    //
    std::string filePath;
    uint32_t filePosPointer;
    int fanotifyFd;
    int fileDesc;
    int openOps = 0;
};




void networkEventLoop(io_uring* ring, int *eventChanFd) {
   //gotta wipe theese
    int clientFd = -1;

    
    //figure out how to use IORING_SETUP_SQPOLL
    struct io_uring ring;
    if(io_uring_queue_init(8192, ring, 0) < 0){
        std::cout << "Liburing ring init failed" << std::endl;
        return;
    }

    bufferPool* bPool = new bufferPool(100, 16);

    //this is the hashmap for disk uploads
    std::unordered_map<int32_t, fileOp> activeFilePulls;

    
    globalCBctx = {bPool, &activeFilePulls, ring, *eventChanFd, &clientFd};
   
    
    nghttp2_session_callbacks* callbacks;
    nghttp2_session_callbacks_new(&callbacks);
    nghttp2_session* session;
    nghttp2_session_client_new(&session, callbacks, &globalCBctx);
    

    nghttp2_session_callbacks_set_on_frame_recv_callback(callbacks, [](nghttp2_session *session, const nghttp2_frame *frame, void *user_data) -> int {
        std::cout << "on_frame_recv_callback: " << frame->hd.type << "\n";

        //ugly cast but eh
        std::unordered_map<int32_t, fileOp>* activePulls = static_cast<parseCallbackCtx*>(user_data)->activeFilePulls;

        if(frame->headers.cat == NGHTTP2_HCAT_RESPONSE){
            //this is where the 200 OK ought to result in a state machine reservation for an incoming download request
            std::cout << "server side 200 on file pull" << std::endl;

            if(activePulls->find(frame->hd.stream_id) == activePulls->end()){
                frame->hd.stream_id;
                activePulls->emplace(frame->hd.stream_id, fileOp{{},0u, 0});
            }
            
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
    });


    //dont know what to use this for, might have to start thinking about it when i decide to do the server
    
    auto reworkedWriter = [](nghttp2_session *session, 
        uint8_t flags, int32_t stream_id, 
        const uint8_t *data, size_t len, void *user_data) -> int {



        auto ctx = static_cast<parseCallbackCtx*>(user_data);
        
        auto fOp = &ctx->activeFilePulls->find(stream_id)->second;

        if(fOp->fileDesc == -1){
            fOp->fileDesc = open(fOp->filePath.c_str(), O_WRONLY, 0644);
        }

        struct stat fileStat;
        fstat(fOp->fileDesc, &fileStat);
        // for now 4 separate SQEs are the lower bound for a small buf pool
        if(fileStat.st_size < 16392*8){
            //append-slab for a small file, 16kb buffer a piece
            
            //first operation init
            if(fOp->openOps == -1){
                fOp->openOps = fileStat.st_size / 16392;
                fOp->openOps += fileStat.st_size % 16392;
            }

            if(len<=16392){
                if(fOp->frameBuffer.empty()){
                    slab* slb = ctx->bPool->getSlab();
                    fOp->frameBuffer.push_back(slb);
                }
                
                slab* slb = fOp->frameBuffer.back();

                memcpy(slb->data + slb->written, data, len);
                slb->written += len;

                fOp->frameBuffer.push_back(slb);
                
            }else{
                int divCount = len / 16392;
                // this is complete garbage, one large file can block out everything without even using it 
                for(int i = 0; i < divCount; i++){

                    slab* slb = ctx->bPool->getSlab();
                    memcpy(slb->data + slb->written, data + i * 16392, 16392);
                    slb->written += 16392;
                    fOp->frameBuffer.push_back(slb);
                }

                if(len - divCount * 16392 > 0){
                    slab* slb = ctx->bPool->getSlab();
                    memcpy(slb->data + slb->written, data + divCount * 16392, len - divCount * 16392);
                    slb->written += len - divCount * 16392;
                    fOp->frameBuffer.push_back(slb);
                }
            }



        }
        
        //bigger buffer range if needed to add later



        iovec iov[fOp->frameBuffer.size()];
        for(auto it : fOp->frameBuffer){
            if(it->written == 0){ continue; }
            if(it->written < sizeof(it->data)){ throw std::runtime_error("Slab was nullptr in chunk_recv"); }

            //this is the full buffer path
            if(it->written == sizeof(it->data)){

                pointOffset pOff = chunkDecode(fOp->frameBuffer.back()->data, fOp->frameBuffer.back()->written);
                
                io_uring_sqe* sqe = io_uring_get_sqe(ctx->ring);
                io_uring_prep_write(sqe, fOp->fileDesc, fOp->frameBuffer.back()->data + pOff.trimLength, fOp->frameBuffer.back()->written - pOff.trimLength, pOff.offsetPtr);
                io_uring_sqe_set_data(sqe, &fOp);
            
                io_uring_submit(ctx->ring);
            }

            //this is the equal to fatruncate record for files under the buffer size
            //one open op set statically means its under buffer size anyhow
            if((it->written < sizeof(it->data)) and (fOp->openOps == 1)){
                pointOffset pOff = chunkDecode(fOp->frameBuffer.back()->data, fOp->frameBuffer.back()->written);
                
                io_uring_sqe* sqe = io_uring_get_sqe(ctx->ring);
                io_uring_prep_write(sqe, fOp->fileDesc, fOp->frameBuffer.back()->data + pOff.trimLength, fOp->frameBuffer.back()->written - pOff.trimLength, pOff.offsetPtr);
                io_uring_sqe_set_data(sqe, &fOp);
                
                io_uring_submit(ctx->ring);
            }
        }


    };

    nghttp2_session_callbacks_set_on_data_chunk_recv_callback(callbacks, );




    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8080);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    clientFd = socket(AF_INET, SOCK_STREAM, 0);
    

    connect(clientFd, (struct sockaddr*)&addr, sizeof(addr));

    



    int epollfd = epoll_create1(0);
    epoll_ctl(epollfd, EPOLL_CTL_ADD, clientFd, nullptr);
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




        //alternate control path for doing initial registration of file pulls with path
        if(event.data.fd == fanotifyToNetwork){
            eventfd_t chanMsg;
            eventfd_read(fanotifyToNetwork, &chanMsg);
            auto it = reinterpret_cast<int*>(chanMsg);
            auto filePathOpt = resolveFdPath(*it);

            if(filePathOpt){
                std::cout << "file path: " << *filePathOpt << std::endl;
            } else {
                std::cout << "file path could not be resolved for fd: " << *it << std::endl;
                throw std::runtime_error("file path could not be resolved for fd: " + std::to_string(*it));
            }

            int32_t streamId = nghttp2_submit_request(session, nullptr, hdrs, sizeof(hdrs), nullptr, nullptr);
            
            /*
            fileOp newOp(
                filePathOpt.value(), //file path
                0, //file pos pointer
                *it, //fanotify file descriptor
                0, // liburing file descriptor for populating content
                0 //ongoing liburing writes semaphore
            );
            */

            //activeFilePulls.insert({streamId, newOp});
        }



        for(int x = 0; x < 4; x++){
            ssize_t readNum = read(clientFd, buffer, sizeof(buffer));
            if(readNum <= 0){
                break;
            }

            //will rely on the internal buffers to memcpy, slabs will be pushed to callback management so i can accuratley track offsets
            std::cout << "read " << readNum << " bytes\n";
            nghttp2_session_mem_recv(session, reinterpret_cast<const uint8_t*>(buffer), readNum);
        
        }
        
    }

}
