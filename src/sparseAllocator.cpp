#include "sparseAllocator.hpp"



sparseAllocator::sparseAllocator(std::string rootPath,std::vector<fileEntry> &paths){

    if(rootPath.length() < 0 ){
        throw new std::exception;
    }

    for(fileEntry entry: paths){
        sparseEntry tmp;
        tmp.size = entry.fSize;



        this->fileLists.insert({entry.pathName,tmp});
    }


}



void sparseAllocator::createSparsePath(std::string path, int size){
    int fd = ::open(path.c_str(), O_CREAT | O_RDWR | O_TRUNC, 0644);
    if (fd == -1) {
        throw new std::exception;
        return;
    }

    if (::ftruncate(fd, size) == -1) {
        throw new std::exception;
        ::close(fd);
        return;
    }

    ::close(fd);
 }


 bool sparseAllocator::verifyFilePrsesnce(std::string toCheck){
    if(this->fileLists.find(toCheck) != this->fileLists.end()){
        return true;
    }
    return false;
 }


 