// Copyright (c) 2025 Sean Bremner
// Licensed under the MIT License. See LICENSE file for details.

#ifndef MASTER_H
#define MASTER_H

#include "string.h"
#include "stdbool.h"
#include "stdint.h"
#include "microbus.h"
#include "txManager.h"
#include "networkManager.h"
#include "scheduler.h"
#include "masterRx.h"
#include "masterTx.h"

typedef struct {
    tSchedulerState scheduler; // Calcs which nodes get to transmit when
    tNetworkManager nwManager; // Handles nodes joining and leaving the network
    tMasterRx rx;
    tMasterTx tx;
    tNodeStats stats;

    // Schedule
    tNodeIndex currentTxNodeId;
    tNodeIndex nextTxNodeId[MAX_TX_NODES_SCHEDULED+2];

    // State of connected nodes
    uint8_t masterNodeTimeToLive[MAX_NODES]; // this is decremented every SLOTS_TO_LIVE_DECREMENT_COUNT
    tNodeQueue activeNodes; // MAX_NODES
    tNodeQueue nodeTxNodes; // MAX_NODES
    tNodeQueue activeTxNodes; // MAX_NODES
} tMaster;

// Dual channel pipelined - run by interrupt thread!
void masterDualChannelPipelinedPreProcess(tMaster * master, tPacket ** txPacket, tPacket ** rxPacketMemory, bool crcError);
uint8_t masterDualChannelPipelinedPostProcess(tMaster * master); // return numTxFreed

// Single channel - no pipeline
tPacket * masterGetRxPacketMemory(tMaster * master);
uint8_t masterNoDelaySingleChannelProcessRx(tMaster * master, bool crcError); // return numTxFreed
uint8_t masterNoDelaySingleChannelProcessTx(tMaster * master, tPacket ** txPacket); // return numTxFreed

// Called by timer
void masterUpdateTimeUs(tMaster * master, uint32_t usIncr);

// Called by main thread
void masterInit(
    tMaster * master,
    uint8_t numTxNodesScheduled, // max gap between master tx slots -1
    uint8_t maxTxPacketEntries,
    tPacketEntry txPacketEntries[],
    uint8_t maxRxPacketEntries,
    tPacketEntry rxPacketEntries[],
    tPacketEntry * rxPacketQueue[]
);

uint8_t * masterAllocateTxPacket(void * master);
void masterSubmitAllocatedTxPacket(void * master, tNodeIndex dstNodeId, uint16_t numBytes);
uint8_t * masterPeekNextRxDataPacket(void * master, uint16_t * size, tNodeIndex * srcNodeId);
bool masterPopNextDataPacket(void * master);
void getConnectedNodesBitField(void * master, uint8_t connectedNodesBitfield[MAX_NODES/8]);
void masterResetTxCredits(void * master);

#endif