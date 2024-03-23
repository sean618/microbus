#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

#include "string.h"
#include "stdbool.h"
#include "stdint.h"
#include "stdio.h"
#include "stdlib.h"
#include "microbus.h"
#include "txManager.h"

#define MAX_SLOTS_TILL_TX_REQUIRED 40 // Must hear from a node once in this many times it is given an opportunity to transmit
#define MAX_SLOTS_TILL_RX_REQUIRED (MAX_SLOTS_TILL_TX_REQUIRED + 5)
//#define NODE_NEWLY_ALLOCATED 0xFF // TODO: hacky


#define MAX_NODES_ALLOCATED_AT_ONCE 10 // 9 bytes each
typedef struct {
    uint64_t newNodeUniqueId[MAX_NODES_ALLOCATED_AT_ONCE];
    uint8_t newNodeId[MAX_NODES_ALLOCATED_AT_ONCE];
    uint8_t numNewNodes;
} tNetworkManager;


void recordNodeTxChance(uint8_t * nodeTTL, uint8_t maxRxNodes, tNodeIndex rxNodeId, tTxManager * txManager, uint8_t * rxPacketsStart, uint8_t * rxPacketsEnd);
void recordPacketRecieved(tNetworkManager * nwManager, uint8_t * nodeTTL, uint8_t maxRxNodes, tNodeIndex rxNodeId);
tPacket * txNewNodeRequest(tPacket * packet, uint64_t uniqueId);
void rxNewNodePacketRequest(tNetworkManager * nwManager, uint8_t * nodeTTL, uint8_t maxRxNodes, tPacket * packet);
tPacket * txNewNodeResponse(tNetworkManager * nwManager, tPacket * packet);
void rxNewNodePacketResponse(tPacket * packet, uint64_t uniqueId, uint8_t * nodeId);

// tPacket * txNewNodeRequest(tNode * node);
// void rxNewNodePacketRequest(tMasterNode * master, tPacket * packet);
// tPacket * txNewNodeResponse(tMasterNode * master);
// void rxNewNodePacketResponse(tNode * node, tPacket * packet);

#endif