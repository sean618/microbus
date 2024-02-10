#ifndef NODE_H
#define NODE_H

#include "string.h"
#include "stdbool.h"
#include "stdint.h"
#include "scheduler.h"


typedef struct {
    // Common
    uint64_t uniqueId;
    tNodeIndex nodeId;
    bool tx;

    uint8_t txSeqNum;
    uint8_t rxSeqNum;

    // Tx buffer - circular buffer
    uint32_t 
    uint32_t numTxPackets;
    tPacket txPacket[MAX_TX_PACKETS];
    uint32_t txPacketsSent;
    // Rx buffer - a circular buffer to allow access by other threads
    tPacket rxPacket[MAX_RX_PACKETS];
    uint32_t rxPacketsReceived;
    
    // Stats
    //uint32_t txPacketsSent;
    //uint32_t rxPacketsReceived;
    
    // Preallocated memory
    tPacket newNodeTxPacket;
    
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