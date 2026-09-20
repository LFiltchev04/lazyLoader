#include <sys/epoll.h>
#include <sys/fanotify.h>
#include <unistd.h>

#include <cerrno>
#include <climits>
#include <cstring>
#include <fcntl.h>
#include <iostream>

#include "src/addMarks.hpp"
#include "src/containerClass.hpp"
int main(int argc, char* argv[]) {



    //container c;

    //c.determineLayer("/mnt/btrfs/containerdStore/io.containerd.snapshotter.v1.btrfs/active/127/usr/local/lib/redis/modules/redisbloom.so");

    return 0;


    const char* watchPath = (argc > 1) ? argv[1] : ".";

    std::cout << "watch path: " << watchPath <<std::endl;
    int fanFd = fanotify_init(FAN_CLASS_CONTENT, O_RDONLY | O_LARGEFILE | O_NONBLOCK);
    if (fanFd < 0) {
        std::perror("fanotify_init");
        return 1;
    }

    monitorFlagTree(watchPath, fanFd);
    int epFd = epoll_create1(0);
    if (epFd < 0) {
        std::perror("epoll_create1");
        close(fanFd);
        return 1;
    }

    epoll_event ev{};
    ev.events = EPOLLIN;
    ev.data.fd = fanFd;
    if (epoll_ctl(epFd, EPOLL_CTL_ADD, fanFd, &ev) < 0) {
        std::perror("epoll_ctl");
        close(epFd);
        close(fanFd);
        return 1;
    }

    char buf[4096];
    epoll_event events[8];

    while (true) {
        int n = epoll_wait(epFd, events, 8, -1);
        std::cout << "unblocked" <<std::endl;
        if (n < 0) {
            if (errno == EINTR) continue;
            std::perror("epoll_wait");
            break;
        }

        for (int i = 0; i < n; ++i) {
            if (events[i].data.fd != fanFd) continue;

            ssize_t len = read(fanFd, buf, sizeof(buf));
            if (len <= 0) {
                if (errno == EINTR) continue;
                std::perror("read fanotify");
                continue;
            }

            for (fanotify_event_metadata* meta = reinterpret_cast<fanotify_event_metadata*>(buf);FAN_EVENT_OK(meta, static_cast<int>(len));meta = FAN_EVENT_NEXT(meta, len)) {
                std::cout << "event meta is: " << meta->mask <<std::endl;
                if (meta->vers != FANOTIFY_METADATA_VERSION) {
                    std::cerr << "fanotify metadata version mismatch\n";
                    continue;
                }

                if (meta->fd < 0) continue;

                char linkPath[64];
                char filePath[PATH_MAX];
                std::snprintf(linkPath, sizeof(linkPath), "/proc/self/fd/%d", meta->fd);

                ssize_t pathLen = readlink(linkPath, filePath, sizeof(filePath) - 1);
                if (pathLen >= 0) {
                    filePath[pathLen] = '\0';
                    

                    std::cout << "catch all: " << filePath << "\n";
                    
                } else {
                    std::cout << "OPEN: <unknown>\n";
                }

                close(meta->fd);
            }
        }
    }

    close(epFd);
    close(fanFd);
    return 0;
}