#include <nghttp2/nghttp2.h>
#include <string>

#define MAKE_NV(NAME, VALUE) \
    { (uint8_t *)(NAME), (uint8_t *)(VALUE), sizeof(NAME) - 1, sizeof(VALUE) - 1, NGHTTP2_NV_FLAG_NONE }


void sendHeaders(nghttp2_session *session, std::string filePath, int outgoingFd){

    nghttp2_nv nvArr[] = {
        MAKE_NV(":method", "GET"),
        MAKE_NV(":path", filePath.c_str())
    };

    ssize_t writeHere = nghttp2_submit_request(session, nullptr, nvArr, 2, nullptr, nullptr);

    const uint8_t *data = nullptr;

    ssize_t messageWrites = sizeof(nvArr);
    ssize_t serializeWrites = sizeof(nvArr);
    ssize_t delta = 0;

    //just makes sure the message went out properly
    while((messageWrites > 0) and (serializeWrites > 0)){
        if(messageWrites > 0){
            delta = nghttp2_session_mem_send(session, &data);
            if(delta <= 0){
                printf("Sync writer screwed up irrecoverably in mem_send portion\n");
                break;
            }
            serializeWrites -= delta;
        }

        if(serializeWrites > 0){
            delta = nghttp2_session_send(session);
            if(delta <= 0){
                printf("Sync writer screwed up irrecoverably in send portion\n");
                break;
            }
            messageWrites -= delta;
        }
    }
        
}