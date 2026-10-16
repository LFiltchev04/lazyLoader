#include <linux/userfaultfd.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <vector>
#include <iostream>
#include <cstring>
#include <cerrno>
#include <sys/ioctl.h>


int mmapTrapFd(int flags) {
    return syscall(__NR_userfaultfd, flags);
}

//thats a pain, i need a separate binary to make it work
//goddamnit
void fault_handler_thread(int uffd_fd, long page_size, int position) {
    static std::vector<char> page_buffer(page_size);

    while (true) {
        //----- from here
        struct uffd_msg msg;
        ssize_t nread = read(uffd_fd, &msg, sizeof(msg));

        if (nread == 0) {
            std::cout << "[Handler] EOF on userfaultfd\n";
            break;
        }

        if (nread < 0) {
            if (errno == EAGAIN) continue;
            perror("[Handler] read userfaultfd");
            break;
        }


        // Handle page fault events
        if (msg.event == UFFD_EVENT_PAGEFAULT) {
            uint64_t fault_addr = msg.arg.pagefault.address;
            uint64_t page_aligned_addr = fault_addr & ~(page_size - 1);
        //----- down to here
        //is the first sequence of the epoll, it hits the network 
            std::cout << "[Handler] Page fault at address: 0x" 
                      << std::hex << fault_addr 
                      << " (page base: 0x" << page_aligned_addr << ")\n" << std::dec;

            // Prepare page content to resolve the fault
            const char* sample_data = "Hello from the userfaultfd handler thread!";
            


            // Copy data into the faulting page and resume execution
            struct uffdio_copy uffdio_copy;
            uffdio_copy.dst = page_aligned_addr;
            uffdio_copy.src = reinterpret_cast<uint64_t>(page_buffer.data());
            uffdio_copy.len = page_size;
            uffdio_copy.mode = 0;
            uffdio_copy.copy = 0;

            if (ioctl(uffd_fd, UFFDIO_COPY, &uffdio_copy) == -1) {
                perror("[Handler] ioctl UFFDIO_COPY");
                break;
            }

            std::cout << "[Handler] Resolved fault using UFFDIO_COPY (" 
                      << uffdio_copy.copy << " bytes copied)\n";
        }
    }
}