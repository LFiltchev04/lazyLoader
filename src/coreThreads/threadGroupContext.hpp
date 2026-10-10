#pragma once

//class to hold the needed pipe contexts to allow the epoll loops ot notify each other of access events
//also used to start and schedule the core thread groups to peroform file pulling
class threadGroupintercommContext{
    int fanotifyToNetwork[2];
    int uringToFanotify[2];

    public:
        int* getFanotifyToNetwork() {
            return fanotifyToNetwork;
        }
        int* getUringToFanotify() {
            return uringToFanotify;
        }
};