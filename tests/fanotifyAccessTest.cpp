#include <gtest/gtest.h>

#include <cerrno>
#include <thread>
#include <unistd.h>


#include "../src/coreThreads/fileAccessLoop.hpp"
#include "../src/coreThreads/fanotifyNetworkInit.hpp"
#include "../src/coreThreads/uringCompletionEmit.hpp"



TEST(FanotifyAccessTest, fanotifyThreadTest) {
    int fanotifyToNetwork[2];
    int uringToFanotify[2];

    int res = pipe(fanotifyToNetwork);
    ASSERT_EQ((res>=0), true);

    res = pipe(uringToFanotify);
    ASSERT_EQ((res>=0), true);

    const std::string tmpRoot = "/home/llf/tmp";
    const std::string baseDir = "/home/llf/tmp/fanotifyTest";
    const std::string nestedDir = "/home/llf/tmp/fanotifyTest/nestedDir";

    if (mkdir(tmpRoot.c_str(), 0777) != 0 && errno != EEXIST) {
        ASSERT_EQ(errno, 0) << "mkdir(" << tmpRoot << ") failed";
    }
    chmod(tmpRoot.c_str(), 0777);

    if (mkdir(baseDir.c_str(), 0777) != 0 && errno != EEXIST) {
        ASSERT_EQ(errno, 0) << "mkdir(" << baseDir << ") failed";
    }
    chmod(baseDir.c_str(), 0777);

    if (mkdir(nestedDir.c_str(), 0777) != 0 && errno != EEXIST) {
        ASSERT_EQ(errno, 0) << "mkdir(" << nestedDir << ") failed";
    }
    chmod(nestedDir.c_str(), 0777);

    res = open((baseDir + "/testFileOne").c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0777);
    ASSERT_EQ((res>=0), true);
    res = close(res);
    ASSERT_EQ((res>=0), true);

    res = open((baseDir + "/testFileTwo").c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0777);
    ASSERT_EQ((res>=0), true);
    res = close(res);
    ASSERT_EQ((res>=0), true);

    res = open((nestedDir + "/nestedFileOne").c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0777);
    ASSERT_EQ((res>=0), true);
    res = close(res);
    ASSERT_EQ((res>=0), true);

    res = open((nestedDir + "/nestedFileTwo").c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0777);
    ASSERT_EQ((res>=0), true);
    res = close(res);
    ASSERT_EQ((res>=0), true);




    //its hella the other way around, how come they read from the start and write from the end 
    // "creat without an e the only mistake" my ass
    std::string basePath("/home/llf/tmp/fanotifyTest");
    std::thread fileAccessThread(fileAccessEventLoop, nullptr, &basePath, &fanotifyToNetwork[1], &uringToFanotify[0]);

    int fdOne = open("/home/llf/tmp/fanotifyTest/testFileOne", O_RDONLY);
    int fdTwo = open("/home/llf/tmp/fanotifyTest/testFileTwo", O_RDONLY);
    

    //trips PRE_CONTENT operations, should get the epoll working internally
    std::thread reader([fdOne]{
        read(fdOne, nullptr, 0);
    });
    reader.detach();
    //should be kept blocked
    EXPECT_EQ((reader.joinable()), false);

    //this ought to just listen for events on the network poke channel
    fanotifyNetworkInit *netInit;
    int pipeReturn = read(fanotifyToNetwork[0], netInit, 0);
    EXPECT_EQ((pipeReturn>=0), true);
    
    int preservedFd = netInit->fanPermFd;
    delete netInit; //has to cleanup the init transports

    //now you just allow the thing on the way back
    uringCompletionEmit *uringEmit = new uringCompletionEmit;
    uringEmit->accFd = preservedFd;
    write(uringToFanotify[1], &uringEmit, sizeof(uringCompletionEmit));

    EXPECT_EQ(reader.joinable(), true);
    reader.join();


    //at this point it ought to be alright to enter cleanup
    EXPECT_EQ((fdOne>=0), true);
    close(fdOne);
    EXPECT_EQ((fdTwo>=0), true);
    close(fdTwo);

    fileAccessThread.join();

    close(fanotifyToNetwork[0]);
    close(fanotifyToNetwork[1]);
    close(uringToFanotify[0]);
    close(uringToFanotify[1]);
    
    unlink("/home/llf/tmp/fanotifyTest/nestedDir/nestedFileOne");
    unlink("/home/llf/tmp/fanotifyTest/nestedDir/nestedFileTwo");
    unlink("/home/llf/tmp/fanotifyTest/testFileOne");
    unlink("/home/llf/tmp/fanotifyTest/testFileTwo");

    rmdir("/home/llf/tmp/fanotifyTest/nestedDir");
    rmdir("/home/llf/tmp/fanotifyTest");
}