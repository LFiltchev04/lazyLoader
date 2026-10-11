#pragma once
#include <string>

struct fanotifyNetworkInit {
    std::string fPath;
    int fanPermFd;
};