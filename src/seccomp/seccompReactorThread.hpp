#include <linux/seccomp.h>
#include <seccomp.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>








void reactorThread(int notify_fd) {
    struct seccomp_notif *req;
    struct seccomp_notif_resp *resp;

    // Allocate safe buffers using libseccomp helpers
    if (seccomp_notify_alloc(&req, &resp) < 0) {
        perror("seccomp_notify_alloc");
        exit(1);
    }

    printf("Seccomp supervisor listening for mmap() calls...\n");

    while (1) {
        // Clear structures
        memset(req, 0, sizeof(*req));
        memset(resp, 0, sizeof(*resp));

        // Wait for an intercepted syscall notification
        if (seccomp_notify_receive(notify_fd, req) < 0) {
            if (errno == EINTR) continue;
            perror("seccomp_notify_receive");
            break;
        }

        // Verify the target process is still alive (prevents race conditions)
        if (seccomp_notify_id_valid(notify_fd, req->id) < 0) {
            continue; 
        }

        // Extract mmap arguments from req->data.args[]
        // mmap arguments: void *addr, size_t length, int prot, int flags, int fd, off_t offset
        unsigned long addr  = req->data.args[0];
        unsigned long len   = req->data.args[1];
        unsigned long prot  = req->data.args[2];
        unsigned long flags = req->data.args[3];


        

        printf("Intercepted mmap: addr=0x%lx, len=%lu, prot=0x%lx, flags=0x%lx (PID: %d)\n",
               addr, len, prot, flags, req->pid);

        resp->id = req->id;

        resp->error = 0;
        resp->val = 0;
        resp->flags = SECCOMP_USER_NOTIF_FLAG_CONTINUE;
        

        // Send the response back to the kernel
        if (seccomp_notify_respond(notify_fd, resp) < 0) {
            if (errno == ENOENT) {
                // The syscall was interrupted or process died; safe to ignore
                continue;
            }
            perror("seccomp_notify_respond");
            break;
        }
    }

    seccomp_notify_free(req, resp);
}