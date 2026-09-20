#include <coroutine>
#include <iostream>
#include <nghttp2/nghttp2.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <unordered_map>
#include <fcntl.h>
#include <liburing.h>

//these includes will annoy me later
#include "../src/smallfilesBuffer.hpp"
#include "../src/largefilesBuffer.hpp"

struct coroutine {
    struct promise_type {
        coroutine get_return_object() {
            return coroutine{std::coroutine_handle<promise_type>::from_promise(*this)};
        }


        std::suspend_never initial_suspend() { return {}; }
        std::suspend_never final_suspend() noexcept { return {}; }
        void return_void() {}
        void unhandled_exception() { std::terminate(); }
    };

    coroutine(std::coroutine_handle<promise_type> h) : handle(h) {}
    promise_type& promise() { return handle.promise(); }
    
    
    bool await_ready(){ 
        return false;
    }
    
    bool await_resume(){
        return false;
    }
    
    bool await_suspend(std::coroutine_handle<> h) {
        handle.resume(); return false;
    }

    void resume() {
        handle.resume();
    }
    


    std::coroutine_handle<promise_type> handle;
};


struct PrintAndSuspend {
    const char* filepath;

    // await_ready: "is the result already available, so we can
    // skip suspending entirely?" Returning false means: yes, suspend.
    bool await_ready() { return false; }

    // await_suspend: called the instant we DO suspend. This is where
    // you'd normally register a callback with epoll, a thread pool,
    // a timer, etc. Here we just print, to make the suspend visible.
    void await_suspend(std::coroutine_handle<> h) {


        std::cout << "  [suspending: " << filepath << "]\n";
        // We don't resume h here — we leave it suspended.
        // main() will resume it manually below.
    }

    // await_resume: called when the coroutine resumes, right as
    // co_await's expression finishes evaluating. Its return value
    // becomes the value of the `co_await ...` expression.
    void await_resume() {
        std::cout << "  [resumed: " << filepath << "]\n";
    }
};


uint8_t bufferPool[16000];


struct fileOp {
    uint32_t filePosPointer;
    int fileDesc;
};


int main(){

    struct io_uring ring;
    if(io_uring_queue_init(64,&ring,0) < 0){
        std::cout << "Liburing ring init failed" << std::endl;
        return 1;
    }
    //this is the hashmap for disk uploads
    std::unordered_map<int32_t, fileOp> activeFilePulls;

    struct parseCallbackCtx{
        std::unordered_map<int32_t, fileOp> *activeFilePulls;
        struct io_uring *ring;
    } globalCBctx = {&activeFilePulls, &ring};
    
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

            frame->hd.stream_id;
            activePulls->insert({frame->hd.stream_id, {0u, 0}});
        }

        if(frame->headers.cat == NGHTTP2_FLAG_END_STREAM){
            std::cout << "server side end of stream on file pull" << std::endl;
            fileOp *fo = &activePulls->find(frame->hd.stream_id)->second;
            
            if(fo->fileDesc != 0){
                close(fo->fileDesc);
            }
            
            activePulls->erase(frame->hd.stream_id);
        }
        
        return 0;
    });


    nghttp2_session_callbacks_set_on_data_chunk_recv_callback(callbacks, [](nghttp2_session *session, uint8_t flags, int32_t stream_id, const uint8_t *data, size_t len, void *user_data) -> int {
        std::cout << "on_data_chunk_recv_callback: " << len << "\n";
        auto& activeFilePulls = *static_cast<parseCallbackCtx*>(user_data)->activeFilePulls;
        auto& ring = *static_cast<parseCallbackCtx*>(user_data)->ring;
        
        
        auto it = activeFilePulls.find(stream_id);
        if(it != activeFilePulls.end()){
            //this is where the 200 OK ought to result in a state machine reservation for an incoming download request
            std::cout << "server side data chunk on file pull" << std::endl;

            if(len == 0){
                std::cout << "file pull complete" << std::endl;
                activeFilePulls.erase(it);
                return 0;
            }
            //data is stupid, its basically a pointer to a byte, i need to iterate the pointer until i get to size_t len to actually read everyting, it looks horridly dangeroud
        
            if(it->second.fileDesc == 0) {
                // Open the file for writing
                it->second.fileDesc = open("output_file", O_APPEND, 0644);
                if (it->second.fileDesc < 0) {
                    std::perror("open");
                    return -1;
                }
            }
        
            for(size_t x = 0; x < len; x++){
                //appends to file off of liburing
                auto sqe = io_uring_get_sqe(&ring);
                io_uring_prep_write(sqe, it->second.fileDesc, &data[x], 1, -1);
                
                //this is blocking, ideally avoid it because all request processing will die until disk commit
                //write(it->second.fileDesc, &data[x], 1);
            }
            

            it->second.filePosPointer += len;
        }
        
        //cleanup logic to dump out the other state on fail of server pull for later
        return 0;
    });


    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8080);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    int client_fd = socket(AF_INET, SOCK_STREAM, 0);
    

    connect(client_fd, (struct sockaddr*)&addr, sizeof(addr));

    int epollfd = epoll_create1(0);
    epoll_ctl(epollfd, EPOLL_CTL_ADD, client_fd, nullptr);


    while(true){
        epoll_event event;
        int n = epoll_wait(epollfd, &event, 1, -1);
        if(n < 0){
            return -1;
        }

        for(int x = 0; x < 4; x++){
            ssize_t readNum = read(client_fd, bufferPool, sizeof(bufferPool));
            if(readNum <= 0){
                break;
            }

            std::cout << "read " << readNum << " bytes\n";
            nghttp2_session_mem_recv(session, reinterpret_cast<const uint8_t*>(bufferPool), readNum);
        
        }
        
    }
}