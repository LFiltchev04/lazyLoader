#ifndef DATABASESINGLETON_HPP
#define DATABASESINGLETON_HPP

#include <string>
#include <coroutine>
#include <unordered_map>
#include <cinttypes>

#include <lmdb.h>
#include "blobState.hpp"


//dont know whether ill need this but i better keep it in case containerd wants to see it, will handle later

class databaseSingleton{
    MDB_env *env;
    MDB_dbi dbi;
    int flushPeriod;

    //std::unordered_map<std::string, blobEntry*> blobTableList;
    //holds the actual FS pointers, want to move the other object into this one entirely
    std::unordered_map<std::string, blobState*> blobStateTable;

    void flushData();
    
    public:
    databaseSingleton();
    ~databaseSingleton();

    void openDatabase(const char* path);
    void closeDatabase();
    //hindsight, this is dangerous
    void putData(const std::string& key, const blobState& value);
    void registerNewBlob(const char key[39]);
    bool blobPresent(const std::string& key);
    void initBaseDB();
};

#endif // DATABASESINGLETON_HPP