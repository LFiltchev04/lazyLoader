#include "addMarks.hpp"

#include <filesystem>
#include <fcntl.h>
#include <sys/fanotify.h>
#include <unistd.h>

namespace fs = std::filesystem;

// Add fanotify marks to each entry in the provided root path.
void monitorFlagTree(std::string treeRoot, int fanotifyFd) {
    fs::directory_iterator start{treeRoot};
    fs::directory_iterator end{};

    for (auto iter{start}; iter != end; ++iter) {
        if (fanotify_mark(fanotifyFd, FAN_MARK_ADD, FAN_OPEN | FAN_EVENT_ON_CHILD | FAN_OPEN_PERM, AT_FDCWD, iter->path().c_str()) < 0) {
            std::perror("fanotify_mark");
            close(fanotifyFd);
            return;
        }
    }
}
