#ifndef TXMANAGER_H
#define TXMANAGER_H

#include "string.h"
#include "stdbool.h"
#include "stdint.h"
#include "microbus.h"

typedef struct {
    bool valid;
    tPacket packet;
} tTxPacketEntry;

typedef struct {
    tTxPacketEntry * entries;
    uint8_t maxEntries;
    uint32_t numStored;
    uint32_t numFreed;
} tPacketStore;

typedef struct {
    tPacket * allocatedPacket;
    tNodeIndex lastTxNode;
    tPacketStore packetStore;
    uint8_t * txSeqNumStart;
    uint8_t * txSeqNumEnd;
    uint8_t * txSeqNumNext;
    uint8_t * rxSeqNum;
    uint8_t numTxNodes;
} tTxManager;

void initTxManager(
    tTxManager * manager,
    uint8_t numTxNodes,
    uint8_t txSeqNumStart[],
    uint8_t txSeqNumEnd[],
    uint8_t txSeqNumNext[],
    uint8_t rxSeqNum[],
    uint8_t numPacketEntries,
    tTxPacketEntry packetEntries[]);
tPacket * allocateTxPacket(tTxManager * manager, uint8_t dstNodeId);
tPacket * getNextTxPacket(tTxManager * manager, uint8_t nextTxNodeId[NUM_TX_NODES_SCHEDULED]);
void rxAckSeqNum(tTxManager * manager, tNodeIndex nodeId, uint8_t ackSeqNum);
bool rxPacketCheckAndUpdateSeqNum(tTxManager * manager, tNodeIndex srcNodeId, uint8_t packetTxSeqNum);
void txManagerRemoveNode(tTxManager * manager, uint8_t nodeId);

#endif
