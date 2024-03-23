#ifndef MASTER_H
#define MASTER_H

#include "string.h"
#include "stdbool.h"
#include "stdint.h"
#include "scheduler.h"
#include "masterTxManager.h"

#define MAX_TX_NODES MAX_NODES

typedef struct {
    // Common
    uint64_t uniqueId;
    tNodeIndex nodeId;
    tMasterSchedulerState scheduler;
    tTxManager txManager;
    // Preallocated memory for new nodes
    tPacket tmpPacket;
    // Tx buffer
    //uint8_t numTxNodes;
    uint8_t txSeqNumStart[MAX_TX_NODES];
    uint8_t txSeqNumEnd[MAX_TX_NODES];
    uint8_t txSeqNumNext[MAX_TX_NODES];
    uint8_t rxSeqNum[MAX_TX_NODES];
    tTxPacketEntry * txPacketEntries;
    // Rx buffer - a circular buffer to allow access by other threads
    uint8_t maxRxPackets;
    tPacket * rxPackets;
    uint8_t rxPacketsStart;
    uint8_t rxPacketsEnd;
    //uint8_t numRxPackets;
    // Stats
    //uint64_t txPacketsSent;
    uint64_t rxPacketsReceived;
    // Simulation only
    tNodeIndex simId;
    uint64_t simTimeNs;
    
    //bool psVal; // TODO: needs a better name
} tMasterNode;

// void initNode(tNode * node, tHalFns halFns, uint8_t maxTxPacketEntries, tTxPacketEntry txPacketEntries[], uint8_t maxRxPackets, tPacket rxPackets[]);
// void nodeTransferCompleteCb(tNode * node);
// uint8_t * allocateNodeTxPacket(tNode * node, tNodeIndex dstNodeId, uint16_t numBytes);


#endif