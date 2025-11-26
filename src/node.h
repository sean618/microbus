// Copyright (c) 2025 Sean Bremner
// Licensed under the MIT License. See LICENSE file for details.

#ifndef NODE_H
#define NODE_H

#include "string.h"
#include "stdbool.h"
#include "stdint.h"
#include "microbus.h"
#include "txManager.h"
#include "rxManager.h"
#include "networkManager.h"
#include "scheduler.h"

typedef struct {
    uint8_t txSeqNumStart;
    uint8_t txSeqNumEnd;
    uint8_t txSeqNumNext;
    uint8_t txSeqNumPauseCount;
    uint8_t rxSeqNum;
} tNodeTxManagerMemory;

typedef struct {
    bool initialised;
    tTxManager txManager; // Handle re-transmission
    tNodeTxManagerMemory txManagerMemory; // Memory used by the tx manager - not used directly
    tRxPacketManager rxPacketManager;
    uint64_t uniqueId;
    tNodeIndex nodeId;
    tNodeIndex currentTxNodeId;
    tNodeIndex nextTxNodeId[MAX_TX_NODES_SCHEDULED+1];
    int32_t timeToLive;
    bool sentNewNodeRequest;
    // uint32_t timeSinceLastHeardMaster;
    uint8_t nextNewNodeResponseCountdown;
    uint8_t newNodeBackoff;
    tPacketEntry * nextRxPacketEntry;
    tPacketEntry * prevRxPacketEntry;
    tPacket * nextTxPacket;
    uint8_t rxBufferLevel;
    bool savedRxAckValid;
    uint8_t savedRxAck;
    bool validRxSeqNum;
    bool validRxPacket;
    tNodeStats stats;
    // Spare memory for sending packets like new node packets
    uint8_t tmpPacketCycle;
    tPacketHeader tmpEmptyPacketHeader[2];
    tPacket tmpPacket;
} tNode;


// Dual channel pipelined
void nodeDualChannelPipelinedPreProcess(tNode * node, tPacket ** txPacket, tPacket ** rxPacketMemory, bool crcError);
void nodeDualChannelPipelinedPostProcess(tNode * node);

// Single channel - no pipeline
bool nodeIsTxMode(tNode * node);
tPacket * nodeGetRxPacketMemory(tNode * node);
void nodeNoDelaySingleChannelProcessRx(tNode * node, bool crcError);
void nodeNoDelaySingleChannelProcessTx(tNode * node, tPacket ** txPacket);

void nodeUpdateTimeUs(tNode * node, uint32_t usIncr);

// Called by main thread
void nodeInit(tNode * node, 
                uint64_t uniqueId,
                uint8_t maxTxPacketEntries, 
                tPacketEntry txPacketEntries[], 
                uint8_t maxRxPacketEntries, 
                tPacketEntry rxPacketEntries[],
                tPacketEntry * rxPacketQueue[]);
uint8_t * nodeAllocateTxPacket(void * node);
void nodeSubmitAllocatedTxPacket(void * node, uint8_t dstNodeId, uint16_t numBytes);
uint8_t * nodePeekNextRxDataPacket(void * node, uint16_t * size, tNodeIndex * srcNodeId);
bool nodePopNextDataPacket(void * node);

tPacket * nodeAllocateTxPacketFull(void * node);
tPacket * nodePeekNextRxDataPacketFull(void * node);

void nodeReset(tNode * node);


#endif