#ifndef CONTAINERCLASS_HPP
#define CONTAINERCLASS_HPP

#include <string>
#include <vector>
#include "httplib.h"
#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/btrfs.h>
#include <sys/ioctl.h>
#include <linux/btrfs_tree.h>
#include <sys/fanotify.h>


#include "blobState.hpp"
#include "databaseSingleton.hpp"


//the container class holds multiple blobState pointers and performs the actual lazy fetching
class container{

    static int fanotifyFd;
    static databaseSingleton* dbSingleton;
    std::vector<blobState*> blobStates;
    std::string containerRoot;

    int layerGenID(std::string layerHash);

    public:
    container(std::vector<blobState*> layers);

    std::string determineLayer(std::string path);
    std::string xattrLayerDerive(std::string path);

    void setupFanotify(std::string treeRoot);
};

#endif // CONTAINERCLASS_HPP