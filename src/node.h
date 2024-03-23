#ifndef NODE_H
#define NODE_H

#include "string.h"
#include "stdbool.h"
#include "stdint.h"
#include "microbus.h"
#include "txManager.h"
#include "networkManager.h"
#include "scheduler.h"

// typedef struct {
//     // Common
//     uint64_t uniqueId;
//     tNodeIndex nodeId;
//     tNodeIndex nextTxNodeId[NUM_TX_NODES_SCHEDULED];
//     // Tx buffer
//     uint8_t txSeqNumStart;
//     uint8_t txSeqNumEnd;
//     uint8_t txSeqNumNext;
//     uint8_t rxSeqNum;
//     tTxPacketEntry * txPacketEntries;
//     tTxManager txManager;
//     // Preallocated memory for new nodes
//     tPacket newNodeTxPacket;
//     // Rx buffer - a circular buffer to allow access by other threads
//     uint8_t maxRxPackets;
//     tPacket * rxPackets;
//     uint8_t rxPacketsStart;
//     uint8_t rxPacketsEnd;
//     //uint8_t numRxPackets;
//     // Stats
//     //uint64_t txPacketsSent;
//     uint64_t rxPacketsReceived;
//     // Simulation only
//     tNodeIndex simId;
//     uint64_t simTimeNs;
// } tNode;

// // Called by interupt
// void nodeProcessRxAndGetTx(tNode * node, tPacket ** txPacket, tPacket ** nextRxPacket);
// // Called by main thread
// void nodeInit(tNode * node, uint8_t maxTxPacketEntries, tTxPacketEntry txPacketEntries[], uint8_t maxRxPackets, tPacket rxPackets[]);
// uint8_t * nodeAllocateTxPacket(tNode * node, tNodeIndex dstNodeId, uint16_t numBytes);
// tPacket * nodePeekNextRxDataPacket(tNode * node);
// void nodePopNextDataPacket(tNode * node);


typedef struct {
    // Common
    bool isMaster;
    uint64_t uniqueId;
    tNodeIndex nodeId;
    tSchedulerState * scheduler; // Only for master - calcs which nodes get to transmit when
    tNetworkManager * nwManager; // Only for master - handles nodes joining and leaving the network
    tNodeIndex nextTxNodeId[NUM_TX_NODES_SCHEDULED];
    tTxManager txManager; // Handle re-transmission
    
    uint8_t ourTTL; // Keep alive - We must send a packet every so often
    uint8_t * nodeTTL; // Keep alive - We must receive a packet every so often
    uint8_t maxRxNodes;
    
    // Preallocated memory for new nodes
    tPacket tmpPacket;
    // Tx buffer
    // //uint8_t numTxNodes;
    // uint8_t txSeqNumStart[MAX_TX_NODES];
    // uint8_t txSeqNumEnd[MAX_TX_NODES];
    // uint8_t txSeqNumNext[MAX_TX_NODES];
    // uint8_t rxSeqNum[MAX_TX_NODES];
    // tTxPacketEntry * txPacketEntries;
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
} tNode;

#endif