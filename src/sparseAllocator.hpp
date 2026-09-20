#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <system_error>
#include <vector>
#include <unordered_map>
#include <string>


struct fileEntry{
    std::string pathName;
    int fSize;
};

struct sparseEntry{
    int size;
    bool presence = false;
};

class sparseAllocator{
    std::unordered_map<std::string,sparseEntry> fileLists;
    std::string rootPath;

    void createSparsePath(std::string path, int size);

    public:
    sparseAllocator(std::string rootPath, std::vector<fileEntry> &paths);
    bool verifyFilePrsesnce(std::string toCheck);


};