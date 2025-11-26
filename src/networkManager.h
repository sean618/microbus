// Copyright (c) 2025 Sean Bremner
// Licensed under the MIT License. See LICENSE file for details.

#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

#include "string.h"
#include "stdbool.h"
#include "stdint.h"
#include "stdio.h"
#include "stdlib.h"
#include "microbus.h"
#include "scheduler.h"
// #include "node.h"

// Node must timeout first
// The timeout is reset everytime the node transmits to the master
#define NODE_TIMEOUT_US (2 * 2 /* ALLOCATION EVERY 2 SLOTS */ * MAX_SLOTS_BETWEEN_SERVICING * MAX_NODES * SLOT_TIME_US)
#define MASTER_TIMEOUT_US (NODE_TIMEOUT_US + (100*SLOT_TIME_US))

// How often we will update the time to live
 // This is to keep the the time to live for each node between 0 and 255 so we don't use too much memory
#define MASTER_MAX_TIME_TO_LIVE  128
#define TIME_TO_LIVE_UPDATE_TIME_US (MASTER_TIMEOUT_US / MASTER_MAX_TIME_TO_LIVE)

// Based on simulations max_nodes/2 gives a good performance
#define MAX_NEW_NODE_BACKOFF (MAX_NODES/2)

#define MAX_NODES_ALLOCATED_AT_ONCE 10

#define REMOVE_NODE_TTL 0xFF

typedef struct {
    uint64_t newNodeUniqueId[MAX_NODES_ALLOCATED_AT_ONCE];
    uint8_t newNodeId[MAX_NODES_ALLOCATED_AT_ONCE];
    uint8_t numNewNodes;
    tNodeQueue * activeNodes;
    uint32_t timeToLiveTimeUs; // Once this counter reaches a certain time decrement all node TTL counts
} tNetworkManager;

// Master only
void networkManagerInit(tNetworkManager * nwManager, tNodeQueue * activeNodes);
void networkManagerRecordRxPacket(tNetworkManager * nwManager, uint8_t nodeTTL[MAX_NODES], tNodeIndex rxNodeId);
bool networkManagerRemoveNewNodeRequest(tNetworkManager * nwManager, tNodeIndex nodeId);
void networkManagerRegisterNewNode(tNetworkManager * nwManager, uint8_t masterNodeTimeToLive[MAX_NODES], uint64_t uniqueId, uint32_t * networkFullCount);
void networkManagerUpdateTimeUs(tNetworkManager * nwManager, uint8_t masterNodeTimeToLive[MAX_NODES], uint32_t usIncr);

// Node only
void nodeNwRecordTxPacketSent(int32_t * timeToLive);
bool nodeNwHasNodeTimedOut(int32_t timeToLive);
void nodeNwUpdateTimeUs(int32_t * timeToLive, uint32_t usIncr);

// Packet specific calls
tPacket * txNewNodeRequest(tPacket * packet, uint64_t uniqueId);
void rxNewNodePacketRequest(tNetworkManager * nwManager, uint8_t masterNodeTimeToLive[MAX_NODES], tPacket * packet, uint32_t * networkFullCount);
void txNewNodeResponse(tNetworkManager * nwManager, tPacket * packet);
void rxNewNodePacketResponse(tPacket * packet, uint64_t uniqueId, tNodeIndex * nodeId, int32_t * timeToLive, uint32_t * statsNodeJoined);



#endif