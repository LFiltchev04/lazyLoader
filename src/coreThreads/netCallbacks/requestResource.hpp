#include <nghttp2/nghttp2.h>
#include <string>
#include <unistd.h>

#define MAKE_NV(NAME, VALUE) \
    { (uint8_t *)(NAME), (uint8_t *)(VALUE), sizeof(NAME) - 1, sizeof(VALUE) - 1, NGHTTP2_NV_FLAG_NONE }


void sendHeaders(nghttp2_session *session, std::string filePath, int outgoingFd, void* userDataCtx){

    nghttp2_nv nvArr[] = {
        MAKE_NV(":method", "GET"),
        MAKE_NV(":path", filePath.c_str())
    };

    //makes sure to init the request first whenever its asked for
    nghttp2_priority_spec priSpec;
    nghttp2_priority_spec_init(&priSpec, 0, 256, 0);

    ssize_t writeHere = nghttp2_submit_request(session, &priSpec, nvArr, 2, nullptr, userDataCtx);

    const uint8_t *data = nullptr;

    ssize_t messageWrites = sizeof(nvArr);
    ssize_t serializeWrites = sizeof(nvArr);
    ssize_t delta = 0;

    //just makes sure the message went out properly
    while((messageWrites > 0) and (serializeWrites > 0)){
        if(serializeWrites > 0){
            delta = nghttp2_session_mem_send(session, &data);
            if(delta <= 0){
                printf("Sync writer screwed up irrecoverably in mem_send portion\n");
                break;
            }
            serializeWrites -= delta;
        }

        //is likely OK given its exclusivley used for small requests that are likely hpacked in, there is a real possibility
        //its actually faster than hoping the uring queue eventually gets to it, priority can be actually enforced that way, not stuck in some queue
        //waiting for some unseen scheduler to see it as fit to dump into the socket
        if(messageWrites > 0){
            delta = write(outgoingFd, data, serializeWrites);
            if(delta <= 0){
                printf("Sync writer screwed up irrecoverably in send portion\n");
                break;
            }
            messageWrites -= delta;
        }
    }

}