// Copyright (c) 2025 Sean Bremner
// Licensed under the MIT License. See LICENSE file for details.

#ifndef TESTSUPPORT_H
#define TESTSUPPORT_H

#include "stdlib.h"
#include "string.h"
#include "assert.h"

#include "../src/microbus.h"
#include "../src/master.h"
#include "../src/node.h"

#include "../src/scheduler.h"
#include "../src/networkManager.h"
#include "../src/txManager.h"

#include "packetChecker.h"

void * myMalloc(size_t numBytes);
tMaster * createMaster(uint32_t txQueueSize, uint32_t rxQueueSize, bool singleChannel);
tNode * createNode(uint32_t txQueueSize, uint32_t rxQueueSize, uint64_t uniqueId);
void freeNode(tNode * node);
void freeMaster(tMaster * master);
void initSystem(tPacketChecker * checker, tMaster ** master, tNode * nodes[MAX_NODES], uint32_t numNodes, bool singleChannel);

void run(tMaster * master, tNode * nodes[], bool ignoreNodes[], uint32_t numNodes, uint32_t numFrames, bool allowNodeTxOverlaps, bool singleChannel);
void runUntilAllNodesOnNetwork(tMaster ** master, tNode * nodes[MAX_NODES], uint32_t numNodes, bool disableLogging, bool singleChannel);
bool attemptMasterTxRandomPacket(tPacketChecker * checker, tMaster * master, tNode * nodes[], uint8_t dstSimNodeId);
bool attemptNodeTxRandomPacket(tPacketChecker * checker, tNode * node, uint8_t srcSimNodeId);
void fillTxBuffersWithRandomPackets(
    uint32_t numPackets,
    tPacketChecker * checker,
    tMaster * master,
    tNode * nodes[MAX_NODES],
    uint32_t numNodes,
    uint32_t packetsSent[MAX_NODES],
    uint32_t * numPacketsSent,
    bool onlyMaster,
    bool onlyFirstNode,
    bool mightBeDropped);
bool areAllTxBuffersEmpty(tMaster * master, tNode * nodes[MAX_NODES], uint32_t numNodes, bool printFalse);
void getMasterRxData(tMaster * master, uint8_t * rxData, uint8_t numBytes);
void getNodeRxData(tNode * node, uint8_t * rxData, uint8_t numBytes);
uint32_t processAllRxData(tPacketChecker * checker, tMaster * master, tNode * nodes[MAX_NODES], uint32_t numNodes);

#endif
