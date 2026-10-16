#pragma once
#include <string>
#include "containerClass.hpp"


struct shmPage{
    int memcpLocation;
    uint32_t len;
    std::string filename;
    int ctrPipeFd;
    container* ctrDb;

    bool signalPageReady(){
        uint8_t signal = 1;
        if(sizeof(uint8_t) == write(ctrPipeFd, &signal, sizeof(signal))){
            return true;
        }
        return false;
    }

    bool backendAlive(){
        uint8_t signal = 0;
        if(sizeof(uint8_t) == read(ctrPipeFd, &signal, sizeof(signal))){
            return true;
        }
        return false;
    }
};
