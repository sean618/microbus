#ifndef MASTER_H
#define MASTER_H

#include "string.h"
#include "stdbool.h"
#include "stdint.h"
#include "scheduler.h"
#include "masterTxManager.h"


typedef struct {
    uint64_t uniqueId;
    tMasterTxManager txManager;
    tMasterSchedulerState scheduler;

    uint8_t numNodes;
    // tNodeIndex nodeId;
    // bool tx;
    

    // Rx buffer - a circular buffer to allow access by other threads
    uint32_t numRxPackets;
    tPacket rxPacket[MAX_RX_PACKETS];
    
    // Preallocated memory
    tPacket tmpPacket;
    
    // Simulation only
    tNodeIndex simId;
    uint64_t simTimeNs;


    //bool psVal; // TODO: needs a better name
} tMasterNode;

#endif