#pragma once
#include <sys/epoll.h>
#include <liburing.h>
#include <iostream>
#include <mutex>
#include "sys/eventfd.h"

#include "fileOp.hpp"
#include "uringCompletionEmit.hpp"


//i do not yet know whether i want to turn this into its separate thread or just integrate it in the fd loop, will see.
void uringCompletionHandler(io_uring& ring, int& uringToFanotify, std::mutex& ringLock) {
    int epfd = epoll_create1(0);    

    printf("Starting uring completion handler\n");
    epoll_event event;
    event.events = EPOLLIN;

    int evfd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    io_uring_register_eventfd(&ring, evfd);

    int epInitCode = epoll_ctl(epfd, EPOLL_CTL_ADD, evfd, &event);
    if(epInitCode < 0){
        std::cout << "epoll_ctl for uring handler failed" << std::endl;
        return;
    }

    io_uring_cqe* cqe;
    while(true){
        

        int n = epoll_wait(epfd, &event, 1, -1);
        if(n < 0){
            std::cout << "epoll_wait failed for uring handler" << std::endl;
            return;
        }

        printf("completion arrived\n");
        unsigned head;
        unsigned int c = 0u;
        ringLock.lock();
        io_uring_for_each_cqe(&ring, head, cqe){

            auto* ctx = static_cast<fileOp*>(io_uring_cqe_get_data(cqe));
            if(cqe->res <= 0){
                std::cout << "io_uring_peek_cqe failed with status: " << static_cast<int>(cqe->res) << std::endl;
            }

            //again, i am assuming it wont short at all so i leave it as is. 
            ctx->openOps--;
            if(ctx->openOps != cqe->res){
                std::cout << "\n\n\n\nMismatch between openOps and cqe->res: " << ctx->openOps << " vs " << cqe->res << "\n\n\n\n";
            }
            

            c++;
        }
        ringLock.unlock();

    }
}