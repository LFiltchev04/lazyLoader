#include <unordered_map>
#include "../../smallfilesBuffer.hpp"


struct parseCallbackCtx{
        bufferPool* bPool;
        struct io_uring *ring;
        int eventChanFd;
        int *tcpClientFd;
};