// Copyright (c) 2025 Sean Bremner
// Licensed under the MIT License. See LICENSE file for details.

#ifndef MASTERTX_H
#define MASTERTX_H

#include "string.h"
#include "stdbool.h"
#include "stdint.h"
#include "microbus.h"
#include "networkManager.h"
#include "scheduler.h"
#include "txManager.h"

typedef struct {
    uint8_t txSeqNumStart[MAX_NODES];
    uint8_t txSeqNumEnd[MAX_NODES];
    uint8_t txSeqNumNext[MAX_NODES];
    uint8_t txSeqNumPauseCount[MAX_NODES];
    uint8_t rxSeqNum[MAX_NODES];
} tMasterTxManagerMemory;

typedef struct {
    tMasterTxManagerMemory txManagerMemory; // Memory used by the tx manager - not used directly
    tPacket * nextTxPacket;
    // Spare memory for sending packets like new node packets
    uint8_t tmpPacketCycle;
    tPacketHeader tmpEmptyPacketHeader[2];
    tPacket tmpPacket;
    tNodeStats * stats;
    uint32_t masterResetCycles;
    tTxManager txManager; // Handles queues for re-transmission
} tMasterTx;

void masterQuickUpdateTxPacket(tMasterTx * tx, tSchedulerState * scheduler, tNodeIndex nextTxNodeId[MAX_TX_NODES_SCHEDULED]);
void masterProcessTx(tMasterTx * tx, tNetworkManager * nwManager, tSchedulerState * scheduler, tNodeIndex nextTxNodeId[MAX_TX_NODES_SCHEDULED]);
tPacket * masterTxGetNextTxPacket(tMasterTx * tx);
void masterTxInit(tMasterTx * tx, tNodeStats * stats, tNodeQueue * activeTxNodes, uint8_t maxTxPacketEntries, tPacketEntry txPacketEntries[]);


#endif