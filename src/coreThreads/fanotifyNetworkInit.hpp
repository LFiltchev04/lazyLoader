#pragma once
#include <string>

struct fanotifyNetworkInit {
    int offset;
    std::string fPath;
    int fanPermFd;
};