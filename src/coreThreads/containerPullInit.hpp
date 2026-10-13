#pragma once
#include <sys/socket.h>
#include <sys/epoll.h>
#include <iostream>


#include "../databaseSingleton.hpp"



//here the pre-allocation happens, it does lmdb calls to persist the state and registers it, is inited by the modified containerd
void pullRequestInit(databaseSingleton* dbSingleton);