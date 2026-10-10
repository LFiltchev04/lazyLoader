#pragma once 
#include "../databaseSingleton.hpp"
#include <unordered_set>
#include <sys/eventfd.h>
#include <linux/fanotify.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <limits.h>
#include <iostream>


void fileAccessEventLoop(databaseSingleton* dbSingleton);