#ifndef BLOBSTATE_HPP
#define BLOBSTATE_HPP

#include <cstdint>
#include <string>
#include <lmdb.h>
#include <sys/xattr.h>

#include "json.hpp"


enum class fileState{
    PRESENT,
    PULLING,
    MISSING
};

enum blobPresence{
    NEW,
    PARTIAL,
    FULL
};

struct fileEntry{
    uint64_t size;
    uint8_t perms;
    fileState state;
};

class blobState{
    static MDB_env *mdb_env;
    blobPresence presence = blobPresence::NEW;

    char blobHash[39];
    bool staticBlob;
    bool pushPreload;
    MDB_dbi tableInstance;

    void fallocateWrapper(std::string path, int size);

    public:
    //used exclusivley for fresh blobState creation
    blobState(const char hash[39]);
    
    blobPresence getBlobState(std::string relPath);
    std::string getBlobHash() const;
    bool isLazyLoadable() const;
    bool isStaticBlob() const;
    bool isPushPreload() const;
    fileState getFileState(std::string relPath);
    
    void setMdbEnv(MDB_env *env);
    void updateFileState(std::string relPath, fileState newState);
    void ingestPreloadList(nlohmann::json jsonArray);

    void materializeBlob();
};

#endif // BLOBSTATE_HPP