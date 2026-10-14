#include <gtest/gtest.h>
#include <thread>
#include <liburing.h>
#include <mutex>
#include <poll.h>

#include "../src/coreThreads/networkReqLoop.hpp"
#include "../src/coreThreads/uringCompletionHandler.hpp"
#include "../src/smallfilesBuffer.hpp"


TEST(networkComponents, basicNetworkRecieve){
    io_uring ring;

    int fanToNet[2];
    pipe(fanToNet);

    bufferPool pool(1024, 64);
    std::mutex ringLock;

    struct io_uring_params params{0};

    params.flags = IORING_SETUP_SQPOLL;
    params.sq_thread_idle = 9000;
    //figure out how to use IORING_SETUP_SQPOLL
    if(io_uring_queue_init_params(8192, &ring, &params) < 0){
        std::cout << "Liburing ring init failed" << std::endl;
        return;
    }


    std::thread serverThread([&](){
        //acts as a remote endpoint for file downloads
        std::system("nghttpd --verbose --no-tls -d / 7070");
    });
    serverThread.detach();
    sleep(1);
    
    EXPECT_FALSE(serverThread.joinable());

    int uringToFan[2];
    pipe(uringToFan);

    std::thread completionHandlerThread([&](){
        uringCompletionHandler(ring, uringToFan[1], ringLock);
    });
    completionHandlerThread.detach();
    sleep(1);
    EXPECT_FALSE(completionHandlerThread.joinable());


    std::thread netparseThread([&](){
        networkEventLoop(&ring, fanToNet[0], "127.0.0.1", &pool, ringLock);
    });
    netparseThread.detach();
    sleep(1);

    //makes sure it didnt blow out of the worker right away
    EXPECT_FALSE(netparseThread.joinable());
   


    //hardcoded paths but meh
    int fd = open("/home/llf/faNotify/tests/tmp/srcFile", O_CREAT | O_WRONLY | O_TRUNC, 0777);
    if(fd < 0){
        std::cout << "Failed to open test file for writing with errcode " << fd << std::endl;
        throw std::runtime_error("Failed to open test file for writing");
    }
    EXPECT_NE(fd, -1);

    std::string testData = "testAaaaaaaaaaaaaaaaagsfgfasdfsdfsxz";
    testData.shrink_to_fit();

    int res = write(fd, testData.c_str(), testData.size());
    EXPECT_EQ(res, testData.size());
    close(fd);

    int tstFd = open("/home/llf/faNotify/tests/tmp/tgtTest", O_RDONLY | O_CREAT | O_TRUNC, 0777);
    EXPECT_NE(tstFd, -1);
    close(tstFd);


    //here the actual network trigger happens
    fanotifyNetworkInit *i = new fanotifyNetworkInit;

    i->fanPermFd = 67;
    i->fPath = "/home/llf/faNotify/tests/tmp/tgtTest";
    write(fanToNet[1], &i, sizeof(fanotifyNetworkInit*));

    std::cout << "Sent network init request" << std::endl;

    sleep(5);
   
    pollfd pfd{uringToFan[0], POLLIN, 0};
    int ready = poll(&pfd, 1, 5000);
    



    std::system("sudo kill $(pgrep nghttpd)");
    //unlink("/home/llf/faNotify/tests/tmp/srcFile");
    //unlink("/home/llf/faNotify/tests/tmp/tgtTest");
    printf("exiting");
}