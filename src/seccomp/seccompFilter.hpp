#include <sys/prctl.h>
#include <seccomp.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>



class seccompFilter{
    std::string linkedHash;
    public:
    seccompFilter(const std::string& hash) : linkedHash(hash) {}

    int seccompFilterInit();
};
