#include "blobState.hpp"
#include "httplib.h"

#include <algorithm>
#include <cstdint>
#include <stdexcept>

MDB_env* blobState::mdb_env = nullptr;

//exclusivley used for socket runtime initialization
blobState::blobState(const char hash[39]){
    if (hash == nullptr) {
        throw std::invalid_argument("Invalid init message, hash is null");
    }

    staticBlob = false;
    pushPreload = true;


    const std::uint8_t firstBool = static_cast<std::uint8_t>(hash[0]);
    const std::uint8_t secondBool = static_cast<std::uint8_t>(hash[1]);
    const std::uint8_t thirdBool = static_cast<std::uint8_t>(hash[2]);

    if (firstBool != 1u) {
        throw std::invalid_argument("Invalid init message, blob isnt seekable");
    }

    staticBlob = (secondBool == static_cast<std::uint8_t>('1'));
    pushPreload = (thirdBool == static_cast<std::uint8_t>('1'));

    const char* actualHash = hash + 3;
    std::copy(actualHash, actualHash + 36, blobHash);

    if(this->pushPreload and (this->presence == blobPresence::NEW)){
        this->presence = blobPresence::PARTIAL;

        httplib::Client client("fixThis");
        httplib::Result res = client.Get("fixPreloadURL");

        nlohmann::json jsonResponse = nlohmann::json::parse(res->body);
        nlohmann::json::array_t fileLists = jsonResponse["fileLists"];

        try{
            ingestPreloadList(fileLists);
        } catch (const std::exception& e) {
            std::cerr << "Failed to ingest preload list: " << e.what() << std::endl;
        }

        

    }
}



fileState blobState::getFileState(std::string relPath){
    MDB_txn *txn;

    MDB_val pathKey{
        .mv_size = relPath.size(),
        .mv_data = const_cast<char*>(relPath.c_str())
    };

    MDB_val returned;
    int errCode = mdb_get(txn, tableInstance, &pathKey, &returned);
    if (errCode != 0) {
        throw std::runtime_error("Failed to get file state from database");
    }

    uint8_t stateValue = *static_cast<uint8_t*>(returned.mv_data);
    switch (stateValue) {
        case 0:
            return fileState::MISSING;
        case 1:
            return fileState::PULLING;
        case 2:
            return fileState::PRESENT;
        default:
            throw std::runtime_error("Invalid file state value in database");
    }
}



void blobState::updateFileState(std::string relPath, fileState newState){
    MDB_txn *txn;

    MDB_val pathKey{
        .mv_size = relPath.size(),
        .mv_data = const_cast<char*>(relPath.c_str())
    };

    uint8_t stateValue;
    switch (newState) {
        case fileState::MISSING:
            stateValue = 0;
            break;
        case fileState::PULLING:
            stateValue = 1;
            break;
        case fileState::PRESENT:
            stateValue = 2;
            break;
        default:
            throw std::invalid_argument("Invalid file state");
    }

    MDB_val newValue{
        .mv_size = sizeof(stateValue),
        .mv_data = &stateValue
    };

    int errCode = mdb_put(txn, tableInstance, &pathKey, &newValue, 0);
    if (errCode != 0) {
        throw std::runtime_error("Failed to update file state in database");
    }
}

void blobState::ingestPreloadList(nlohmann::json jsonArray){
    MDB_txn *txn;
    
    int errCode = mdb_txn_begin(mdb_env, nullptr, 0, &txn);
    if (errCode != 0) {
        throw std::runtime_error("Failed to begin transaction");
    }

    fileState initState = fileState::MISSING;

    for (const auto& file : jsonArray["fileLists"]) {
        uint64_t sz = file["size"].get<std::uint64_t>();
        std::string filePath = file["path"].get<std::string>();
        uint8_t perms = file["perms"].get<std::uint8_t>();

        //this looks like a huge memory leak but it isnt
        MDB_val pathKey{
            .mv_size = filePath.size(),
            .mv_data = const_cast<char*>(filePath.c_str())
        };
        MDB_val pathStatus{
            .mv_size = sizeof(uint8_t),
            .mv_data = static_cast<void*>(&initState)
        };
    }

    int commitErrCode = mdb_txn_commit(txn);
    if (commitErrCode != 0) {
        throw std::runtime_error("Failed to commit transaction");
    }
}


void blobState::fallocateWrapper(std::string path, int size){
    //assume all perms are 0644 but its got to change sometime
    int fd = open(path.c_str(), O_CREAT | O_RDWR | O_TRUNC, 0644);
    if (fd == -1) {
        throw std::system_error(errno, std::generic_category(), "Failed to open file in fallocate");
    }

    if (ftruncate(fd, size) == -1) {
        throw std::system_error(errno, std::generic_category(), "Failed to sparse allocate file");
        close(fd);
        return;
    }

    fsetxattr(fd, "user.layerOrigin", blobHash, sizeof(blobHash), 0);

    close(fd);
 }



void blobState::materializeBlob() {
    //if i do decide to do no-manifest blobs i gotta do some branching here, assumed to be pre-listed for now

    if(pushPreload == true){

        MDB_val pathKey, dataVal;
        MDB_txn *txn;

        MDB_cursor *cursor;
        
        int errCode = mdb_txn_begin(mdb_env, nullptr, 0, &txn);
        if (errCode != 0) {
            throw std::runtime_error("Failed to begin transaction");
        }

        mdb_cursor_open(txn, tableInstance, &cursor);

        for(;;){
            if(cursor == nullptr){
                break;
            }
            
            int crrPosStat = mdb_cursor_get(cursor, &pathKey, &dataVal, MDB_GET_CURRENT);
            std::string path(static_cast<char*>(pathKey.mv_data), pathKey.mv_size);
            fileEntry *fentry = static_cast<fileEntry*>(dataVal.mv_data);
            
            if(fentry->state == fileState::MISSING){
                fallocateWrapper(path, 0);
            }

            int nextPosStat = mdb_cursor_get(cursor, &pathKey, &dataVal, MDB_NEXT);
        }

        mdb_cursor_close(cursor);
        mdb_txn_commit(txn);
        this->presence = PARTIAL;
    }else{
        //will gotaa figure out whether its at all tennable to have it actuall be allowed to do no-manifest blobs
        throw std::invalid_argument("blob not push preloadable, not materializable");
    }



}

blobPresence blobState::getBlobState(std::string relPath){
    return presence;
}

std::string blobState::getBlobHash() const {
    return std::string(blobHash, 36);
}

bool blobState::isLazyLoadable() const {
    return pushPreload;
}

bool blobState::isStaticBlob() const {
    return staticBlob;
}

bool blobState::isPushPreload() const {
    return pushPreload;
}

void blobState::setMdbEnv(MDB_env *env) {
    mdb_env = env;
}



