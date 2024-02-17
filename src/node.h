#ifndef NODE_H
#define NODE_H

#include "string.h"
#include "stdbool.h"
#include "stdint.h"
#include "scheduler.h"
#include "txManager.h"

typedef struct {
    // Common
    uint64_t uniqueId;
    tNodeIndex nodeId;
    //bool tx;
    
    tNodeTxManager txManager;
    // Preallocated memory for new nodes
    tPacket newNodeTxPacket;

    // Rx buffer - a circular buffer to allow access by other threads
    
    tPacket rxPackets[MAX_NODE_RX_PACKETS];
    uint8_t rxPacketsStart;
    uint8_t rxPacketsEnd;
    uint8_t numRxPackets;
    
    // Stats
    //uint64_t txPacketsSent;
    uint64_t rxPacketsReceived;
    
    
    // Simulation only
    tNodeIndex simId;
    uint64_t simTimeNs;
} tNode;



// HAL Callbacks
void slaveTransferCompleteCb(tNode * node); // HAL SPI callback
void masterTransferCompleteCb(tMasterNode * master, bool startSequence); // HAL SPI callback


void psLineInterrupt(tNode * node, bool psVal);
void startMicrobus(tMasterNode * master);
void addMasterTxPacket(tMasterNode * master, tNodeIndex dstNodeId, tPacket * packet);
void addSlaveTxPacket(tNode * node, tPacket * packet);

#endif