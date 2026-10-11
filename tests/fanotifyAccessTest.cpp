#include <gtest/gtest.h>

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

    res = mkdir("/tmp/fanotifyTest", 0777);
    ASSERT_EQ((res>=0), true);

    res = open("/tmp/fanotifyTest/testFileOne", O_CREAT | O_WRONLY, 0777);
    ASSERT_EQ((res>=0), true);
    res = close(res);
    ASSERT_EQ((res>=0), true);

    res = open("/tmp/fanotifyTest/testFileTwo", O_CREAT | O_WRONLY, 0777);
    ASSERT_EQ((res>=0), true);
    res = close(res);
    ASSERT_EQ((res>=0), true);

    res = open("/tmp/fanotifyTest/nestedDir/nestedFileOne", O_CREAT | O_WRONLY, 0777);
    ASSERT_EQ((res>=0), true);
    res = close(res);
    ASSERT_EQ((res>=0), true);

    res = open("/tmp/fanotifyTest/nestedDir/nestedFileTwo", O_CREAT | O_WRONLY, 0777);
    ASSERT_EQ((res>=0), true);
    res = close(res);
    ASSERT_EQ((res>=0), true);

    res = mkdir("/tmp/fanotifyTest/nestedDir", 0777);
    ASSERT_EQ((res>=0), true);


    //its hella the other way around, how come they read from the start and write from the end 
    // "creat without an e the only mistake" my ass
    std::string basePath("/tmp/fanotifyTest");
    std::thread fileAccessThread(fileAccessEventLoop, nullptr, &basePath, &fanotifyToNetwork[1], &uringToFanotify[0]);

    int fd = open("/tmp/fanotifyTest/testFileOne", O_RDONLY);
    
    
    
    ASSERT_EQ((fd>=0), true);
    close(fd);

    //staging cleanup
    fileAccessThread.join();

    close(fanotifyToNetwork[0]);
    close(fanotifyToNetwork[1]);
    close(uringToFanotify[0]);
    close(uringToFanotify[1]);
    
    unlink("/tmp/fanotifyTest/nestedDir/nestedFileOne");
    unlink("/tmp/fanotifyTest/nestedDir/nestedFileTwo");
    unlink("/tmp/fanotifyTest/testFileOne");
    unlink("/tmp/fanotifyTest/testFileTwo");

    rmdir("/tmp/fanotifyTest/nestedDir");
    rmdir("/tmp/fanotifyTest");
}