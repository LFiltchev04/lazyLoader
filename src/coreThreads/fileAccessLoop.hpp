#pragma once 

#include <unordered_set>
#include <sys/epoll.h>
#include <unistd.h>
#include <limits.h>
#include <iostream>
#include <sys/fanotify.h>
#include <optional>
#include <string>
#include <sys/stat.h>

#include "../databaseSingleton.hpp"
#include "../utils/fanotifyResolve.hpp"
#include "uringCompletionEmit.hpp"
#include "../blobState.hpp"
#include "fanotifyNetworkInit.hpp"
#include "../utils/addMarks.hpp"


//maintains the tracking of inflight open operations
//if it corsses a threshold or the difference is significant it enters aggregation and batching mode
inline uint64_t inflightOpens = 0;


//base path is optional, the blobState object carries the data but testing with it in isolation is too much of a hassle
void fileAccessEventLoop(blobState *blobDb, std::string *basePath, int *fanotifyToNetwork, int *uringToFanotify, int *eventLoop);