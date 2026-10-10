#pragma once 

#include <unordered_set>
#include <sys/eventfd.h>
#include <linux/fanotify.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <limits.h>
#include <iostream>
#include <sys/fanotify.h>
#include <optional>

#include "../databaseSingleton.hpp"
#include "../utils/fanotifyResolve.hpp"

void fileAccessEventLoop(databaseSingleton* dbSingleton, int fanotifyToNetwork);