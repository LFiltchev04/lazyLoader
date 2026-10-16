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
    std::string hash;
    std::thread watcherTread;
    int notifyFd;

    public:
    seccompFilter();

    void seccompFilterInit();
    void startWatcher();
};
