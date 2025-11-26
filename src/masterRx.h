// Copyright (c) 2025 Sean Bremner
// Licensed under the MIT License. See LICENSE file for details.

#ifndef MASTERRX_H
#define MASTERRX_H

#include "string.h"
#include "stdbool.h"
#include "stdint.h"
#include "microbus.h"
#include "networkManager.h"
#include "scheduler.h"
#include "txManager.h"
#include "rxManager.h"


typedef struct {
    tRxPacketManager rxPacketManager;
    tPacketEntry * nextRxPacketEntry;
    tPacketEntry * prevRxPacketEntry;
    bool validRxPacket;
    bool validRxSeqNum;
    tNodeStats * stats;
} tMasterRx;

void masterQuickProcessPrevRx(tMasterRx * rx, tNetworkManager * nwManager, tTxManager * txManager, uint8_t masterNodeTimeToLive[MAX_NODES], bool rxCrcError);
uint8_t masterProcessRx(tMasterRx * rx, tNetworkManager * nwManager, tSchedulerState * scheduler, tTxManager * txManager, uint8_t masterNodeTimeToLive[MAX_NODES]);
tPacket * masterRxGetNextPacketMemory(tMasterRx * rx);
void masterRxInit(tMasterRx * rx, uint8_t maxRxPacketEntries, tPacketEntry rxPacketEntries[], tPacketEntry * rxPacketQueue[], tNodeStats * stats);

#endif