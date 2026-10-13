#include <iostream>
#include <thread>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <sys/fanotify.h>
#include <fcntl.h>
#include <liburing.h>
#include <nghttp2/nghttp2.h>
#include <unordered_set>
#include <mutex>
#include <optional>
#include <string>

#include "blobState.hpp"
#include "databaseSingleton.hpp"
#include "smallfilesBuffer.hpp"
#include "filePacker.hpp"
#include "streamDecodeWrite.hpp"

//ive got waaaay too much redundant security checks revisit to drop them, this is the hot path for file loads after all





//need theese globals for cross-thread state machine updates, its a stupid thing to do in the first place but meh
int fanotifyGlobalFd = -1;
int fanotifyToNetwork = -1;
int uringToFanotifyGlobal = -1;
int pullReqCompl = -1;
io_uring globalRing;
std::unordered_map<int32_t, fileOp>* activeFilePullsGlobal;
std::mutex activeFilePullsStateLock;





//goes to proc to resolve the file name, is very convenient because the fanotify file descriptor passes through the eventfd channels without having to keep separate lookups


















































//will have to revisit this



//databaseSingleton* dbSingleton = new databaseSingleton();
//std::thread pullRequestThread(pullRequestInit, dbSingleton);

