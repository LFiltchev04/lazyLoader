#include "seccompFilter.hpp"


seccompFilter::seccompFilter() {
    int fd = open("/etc/mmapHandle/hashID", O_RDONLY);

    char buffer[512];
    read(fd, buffer, sizeof(buffer));
    close(fd);

    hash = std::string(buffer);
}   


void seccompFilter::seccompFilterInit() {
    scmp_filter_ctx ctx;
    int notify_fd;

    ctx = seccomp_init(SCMP_ACT_ALLOW);
    if (!ctx) {
        perror("seccomp_init");
    }

    if (seccomp_rule_add(ctx, SCMP_ACT_NOTIFY, SCMP_SYS(mmap), 0) < 0) {
        perror("seccomp_rule_add mmap");
        seccomp_release(ctx);
    }

    if (seccomp_load(ctx) < 0) {
        perror("seccomp_load");
        seccomp_release(ctx);
    }

    notify_fd = seccomp_notify_fd(ctx);
    seccomp_release(ctx); 

    notifyFd = notify_fd;
}


void seccompFilter::startWatcher() {
    watcherTread = std::thread([this]() {
        reactorThread(notifyFd);
    });
}