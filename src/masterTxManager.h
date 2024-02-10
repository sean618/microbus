#ifndef MASTERTXMANAGER_H
#define MASTERTXMANAGER_H

#include "string.h"
#include "stdbool.h"
#include "stdint.h"
#include "microbus.h"

typedef struct {
    bool valid;
    tPacket packet;
} tTxPacketEntry;

typedef struct {
    uint32_t numTxIn;
    uint32_t numTxOut;
    uint8_t txSeqNumStart[MAX_NODES];
    uint8_t txSeqNumCurrent[MAX_NODES];
    uint8_t txSeqNumEnd[MAX_NODES];
    //uint8_t txWindowSize[MAX_NODES];
    //uint8_t numRxPacketsSinceLastSeqNumUpdate[MAX_NODES];
    uint8_t rxSeqNum[MAX_NODES];
    // Tx buffer - A sort of heap
    tTxPacketEntry txPacket[MAX_MASTER_TX_PACKETS];
    tNodeIndex lastTxNode;
} tMasterTxManager;

tPacket * addTxPacket(tMasterTxManager * manager, tNodeIndex dstNodeId);
tPacket * getNextTxPacket(tMasterTxManager * manager);
void rxAckSeqNum(tMasterTxManager * manager, uint8_t nodeId, uint8_t ackSeqNum);

#endif
