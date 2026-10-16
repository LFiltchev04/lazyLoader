#include "seccompFilter.hpp"

int seccompFilter::seccompFilterInit() {
    scmp_filter_ctx ctx;
    int notify_fd;

    ctx = seccomp_init(SCMP_ACT_ALLOW);
    if (!ctx) {
        perror("seccomp_init");
        return -1;
    }

    if (seccomp_rule_add(ctx, SCMP_ACT_NOTIFY, SCMP_SYS(mmap), 0) < 0) {
        perror("seccomp_rule_add mmap");
        seccomp_release(ctx);
        return -1;
    }

    if (seccomp_load(ctx) < 0) {
        perror("seccomp_load");
        seccomp_release(ctx);
        return -1;
    }

    notify_fd = seccomp_notify_fd(ctx);
    seccomp_release(ctx); 

    return notify_fd;
}