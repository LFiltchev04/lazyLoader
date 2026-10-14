#include <gtest/gtest.h>
#include <thread>
#include <liburing.h>
#include <mutex>


#include "../src/coreThreads/networkReqLoop.hpp"
#include "../src/coreThreads/uringCompletionHandler.hpp"
#include "../src/smallfilesBuffer.hpp"


TEST(networkComponents, basicNetworkRecieve){
    io_uring ring;

    int fanToNet[2];
    pipe(fanToNet);

    bufferPool pool(1024, 64);
    std::mutex ringLock;


    std::thread serverThread([&](){
        //acts as a remote endpoint for file downloads
        std::system("nghttpd --no-tls -d / 7070");
    });

    serverThread.detach();
    ASSERT_FALSE(serverThread.joinable());


    std::thread netparseThread([&](){
        networkEventLoop(&ring, fanToNet[0], "127.0.0.1", &pool, ringLock);
    });

    netparseThread.detach();
    //makes sure it didnt blow out of the worker right away
    ASSERT_FALSE(netparseThread.joinable());

    int uringToFan[2];
    pipe(uringToFan);

    std::thread completionHandlerThread([&](){
        uringCompletionHandler(ring, uringToFan[1], ringLock);
    });
    completionHandlerThread.detach();
    ASSERT_FALSE(completionHandlerThread.joinable());


    //hardcoded paths but meh
    int fd = open("/home/llf/faNotify/tests/tmp/testfile", O_CREAT | O_WRONLY | O_TRUNC, 0777);
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
    write(fanToNet[1], &i, sizeof(fanotifyNetworkInit));

    //cleanup
    std::system("sudo kill $(pgrep nghttpd)");
    serverThread.join();
    netparseThread.join();
    completionHandlerThread.join();
    unlink("/home/llf/faNotify/tests/tmp/testfile");
    unlink("/home/llf/faNotify/tests/tmp/tgtTest");
}