// Copyright (c) 2025 Sean Bremner
// Licensed under the MIT License. See LICENSE file for details.

#ifndef TXMANAGER_H
#define TXMANAGER_H

#include "string.h"
#include "stdbool.h"
#include "stdint.h"
#include "microbus.h"

typedef struct {
    tPacketEntry * entries;
    uint8_t maxEntries;
    uint32_t numStored;
    uint32_t numFreed;
} tPacketStore;

typedef struct {
    tPacket * allocatedPacket;
    tPacketStore packetStore;
    uint8_t * txSeqNumStart;
    uint8_t * txSeqNumEnd;
    uint8_t * txSeqNumNext;
    uint8_t * txSeqNumPauseCount;
    uint8_t * rxSeqNum;
    uint8_t maxTxNodes;
    uint8_t maxTxBufferLevel;
    uint8_t txBufferLevel;
    tNodeQueue * activeTxNodes;
    uint8_t lastTxQueueIndex;
    uint8_t lastTxQueueCount;
} tTxManager;


#define IS_TX_BUFFER_EMPTY(txManager, dstNodeId) (((txManager)->txSeqNumEnd[(dstNodeId)] == (txManager)->txSeqNumStart[(dstNodeId)]))

void initTxManager(
        tTxManager * manager,
        uint8_t maxTxNodes,
        uint8_t txSeqNumStart[],
        uint8_t txSeqNumEnd[],
        uint8_t txSeqNumNext[],
        uint8_t txSeqNumPauseCount[],
        uint8_t rxSeqNum[],
        tNodeQueue * activeTxNodes,
        uint8_t maxPacketEntries,
        tPacketEntry packetEntries[]
    );
tPacket * allocateTxPacket(tTxManager * manager, uint8_t nodeId);
void submitAllocatedTxPacket(tTxManager * manager, bool isMaster, uint8_t * masterDstNodeTTL, tNodeIndex srcNodeId, tNodeIndex dstNodeId, tPacketType packetType, uint16_t dataSize);
tPacket * nodeGetNextTxDataPacket(tTxManager * manager);
tPacket * masterGetNextTxDataPacket(tTxManager * manager, uint8_t numTxNodesScheduled, uint8_t nextTxNodeId[MAX_TX_NODES_SCHEDULED], uint8_t burstSize);
void masterTxManagerRemoveNode(tTxManager * manager, tNodeIndex nodeId, uint8_t * numTxPacketsFreed);
void masterTxClearBuffers(tTxManager * manager);
uint8_t rxAckSeqNum(tTxManager * manager, tNodeIndex srcNodeId, uint8_t ackSeqNum, bool isMaster, uint64_t * statsNumTxWindowRestarts);
bool rxPacketCheckAndUpdateSeqNum(tTxManager * manager, tNodeIndex srcNodeId, uint8_t packetTxSeqNum, bool isMaster);
void txManagerResetNodeSeqNumbers(tTxManager * manager, uint8_t nodeId);
uint8_t getNumInTxBuffer(tTxManager * manager, uint8_t dstNodeId);
uint8_t getNumAllBufferedTxPackets(tTxManager * manager);

#endif
