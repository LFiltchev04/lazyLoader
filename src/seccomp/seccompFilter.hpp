#include <sys/prctl.h>
#include <seccomp.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <thread>

#include "seccompReactorThread.hpp"

class seccompFilter{
    bool watcherFlag = false;
    static std::string hash;
    static std::thread watcherTread;
    std::string file;
    static int notifyFd;

    public:
    seccompFilter();


    void seccompFilterInit();
    void startWatcher();

    void newUFFD(int position, int size);
};
