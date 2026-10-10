#include "fanotifyResolve.hpp"



std::optional<std::string> resolveFdPath(int fd) {
    char linkPath[32];
    char filePath[PATH_MAX];
    std::snprintf(linkPath, sizeof(linkPath), "/proc/self/fd/%d", fd);
    ssize_t pathLen = readlink(linkPath, filePath, sizeof(filePath) - 1);
    if (pathLen < 0) {
        return std::nullopt;
    }
    //construct from length directly, readlink does not null-terminate and this avoids the extra strlen
    return std::string(filePath, static_cast<size_t>(pathLen));
}