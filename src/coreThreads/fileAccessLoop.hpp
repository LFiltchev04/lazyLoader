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
#include "uringCompletionEmit.hpp"
#include "../blobState.hpp"
#include "../fileState.hpp"

void fileAccessEventLoop(blobState &blobDb, int fanotifyToNetwork, int uringToFanotify);