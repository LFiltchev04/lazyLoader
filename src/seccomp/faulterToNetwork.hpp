#include <cstdint>
#include <string>

//this is a simple struct that tells it which file to get and which mmap offset to write the process page in 
//it is scoped PER SINGLE MMAP OPERATION
struct faulterToNetwork{
    const char* file; // the file to get
    uint64_t offset;    // the mmap offset to write the process page in
};


