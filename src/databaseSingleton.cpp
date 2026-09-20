#include "databaseSingleton.hpp"
#include <iostream>

databaseSingleton::databaseSingleton(){
    mdb_env_create(&env);
    mdb_env_set_mapsize(env, 60 * 1024 * 1024);

    unsigned int flags = MDB_NOSYNC;
    mdb_env_open(env, "./lmdb_data", flags, 0664);

    MDB_txn *txn;
    mdb_txn_begin(env, NULL, 0, &txn);
    
    mdb_dbi_open(txn, "rootBlobTable", 0, &dbi);

    MDB_val key, val;

    this->initBaseDB();
}



void databaseSingleton::putData(const std::string& key, const blobState& value){
    MDB_txn *txn;
    mdb_txn_begin(env, NULL, 0, &txn);

    MDB_val mdb_key, mdb_val;
    mdb_key.mv_size = key.size();
    mdb_val.mv_size = sizeof(blobState);
    mdb_val.mv_data = (void*)&value;

    int rc = mdb_put(txn, dbi, &mdb_key, &mdb_val, 0);
    if (rc != 0) {
        std::cerr << "Error putting data: " << mdb_strerror(rc) << std::endl;
        mdb_txn_abort(txn);
        return;
    }


    //blobStateTable[key] = new blobState();

    mdb_txn_commit(txn);
}


void databaseSingleton::initBaseDB(){
    MDB_txn *txn;
    mdb_txn_begin(env, NULL, 0, &txn);
    
    mdb_dbi_open(txn, "rootBlobTable", 0, &dbi);

    MDB_val key, val;

    int rc;
    MDB_cursor *cursor;
    
    mdb_cursor_open(txn, dbi, &cursor);
    while ((rc = mdb_cursor_get(cursor, &key, &val, MDB_NEXT)) == 0) {
        std::string k((char*)key.mv_data, key.mv_size);
        blobState *v = static_cast<blobState*>(val.mv_data);
        blobState *tmp = new blobState(*v);
        blobStateTable[k] = tmp;
    }
    mdb_txn_commit(txn);
    mdb_cursor_close(cursor);
}

bool databaseSingleton::blobPresent(const std::string& key){
    return blobStateTable.find(key) != blobStateTable.end();
}

void databaseSingleton::flushData(){
    //will basically just be an async sleep and then commit in background

}




void databaseSingleton::registerNewBlob(const char key[39]){
    
    MDB_txn *txn;
    mdb_txn_begin(env, NULL, 0, &txn);

    MDB_val verifyKey;
    int res = mdb_get(txn, dbi, &verifyKey, nullptr);
    if(res == 0){
        std::cerr << "Blob with key " << key << " already exists in the database." << std::endl;
        mdb_txn_abort(txn);
        throw std::runtime_error("Blob already exists in the database.");
    }

    blobState* newBstate = new blobState(key);

    MDB_val mdb_key, mdb_val;
    mdb_key.mv_size = 39;
    mdb_key.mv_data = (void*)key;
    mdb_val.mv_size = sizeof(blobState);
    mdb_val.mv_data = (void*)newBstate;

    int rc = mdb_put(txn, dbi, &mdb_key, &mdb_val, 0);
    if (rc != 0) {
        std::cerr << "Error putting data: " << mdb_strerror(rc) << std::endl;
        delete static_cast<blobState*>(mdb_val.mv_data);
        
        mdb_txn_abort(txn);
        return;
    }


    mdb_txn_commit(txn);


    this->blobStateTable[key] = newBstate;
    //goddamn stupid ass goofy forever temporary fixes i just goddamn cant, i damn know this is staying here forever
    this->blobStateTable[key]->materializeBlob();

}