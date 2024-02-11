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
    tTxPacketEntry entries[MAX_MASTER_TX_PACKETS];
    uint32_t numStored;
    uint32_t numFreed;
} tPacketStore;

typedef struct {
    tNodeIndex lastTxNode;
    tPacketStore packetStore;
    uint8_t txSeqNumStart[MAX_NODES];
    uint8_t txSeqNumEnd[MAX_NODES];
    uint8_t txSeqNumNext[MAX_NODES];
    uint8_t rxSeqNum[MAX_NODES];
} tMasterTxManager;

typedef struct {
    tPacketStore packetStore;
    uint8_t txSeqNumStart;
    uint8_t txSeqNumEnd;
    uint8_t txSeqNumNext;
    uint8_t rxSeqNum;
} tNodeTxManager;

tPacket * nodeGetNextTxPacket(tNodeTxManager * manager, uint8_t nodeId);
tPacket * nodeAllocateTxPacket(tNodeTxManager * manager, tNodeIndex nodeId);
void nodeRxAckSeqNum(tNodeTxManager * manager, tNodeIndex nodeId, uint8_t ackSeqNum);

tPacket * masterGetNextTxPacket(tMasterTxManager * manager);
tPacket * masterAllocateTxPacket(tMasterTxManager * manager, tNodeIndex nodeId);
void masterRxAckSeqNum(tMasterTxManager * manager, tNodeIndex nodeId, uint8_t ackSeqNum);


#endif
