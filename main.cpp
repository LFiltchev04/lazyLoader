#include <sys/epoll.h>
#include <sys/fanotify.h>
#include <unistd.h>
#include <liburing.h>
#include <cerrno>
#include <climits>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <thread>
#include <chrono>

#include "src/containerClass.hpp"
#include "coreThreads/containerPullInit.hpp"
#include "coreThreads/fileAccessLoop.hpp"
#include "coreThreads/networkReqLoop.hpp"

int main(int argc, char* argv[]) {

    printf("ran");
    int fanToNet[2];
    pipe(fanToNet);

    int uringToFan[2];
    pipe(uringToFan);

    int globalEpfdFile = 0;

  

    io_uring ring;
    io_uring_params params;
    params.flags |= IORING_SETUP_SQPOLL;
    io_uring_queue_init_params(4096, &ring, &params);
    params.sq_thread_idle = 2000;

    bufferPool pool(10);

    std::thread networkThread(networkEventLoop, &ring, fanToNet[0], "127.0.0.1", &pool);
    networkThread.detach();


    std::string basePath = "/home/llf/faNotify/pullabledir";
    std::thread fileAccessThread(fileAccessEventLoop, nullptr, &basePath, &fanToNet[1], &uringToFan[0], &globalEpfdFile);
    fileAccessThread.detach();


    //sleep forever
    while (true) {
        std::this_thread::sleep_for(std::chrono::hours(24));
    }

    return 0;
}