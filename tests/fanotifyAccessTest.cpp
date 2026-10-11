#include <gtest/gtest.h>

#include <thread>


#include "../src/coreThreads/fileAccessLoop.hpp"
#include "../src/coreThreads/fanotifyNetworkInit.hpp"
#include "../src/coreThreads/uringCompletionEmit.hpp"



TEST(FanotifyAccessTest, fanotifyThreadTest) {
    int fanotifyToNetwork[2];
    int uringToFanotify[2];

    pipe(fanotifyToNetwork);
    pipe(uringToFanotify);

    
    //its hella the other way around, how come they read from the start and write from the end 
    // "creat without an e the only mistake" my ass 
    std::thread fileAccessThread(fileAccessEventLoop, nullptr, &fanotifyToNetwork[1], &uringToFanotify[0]);



}