// Copyright (c) 2025 Sean Bremner
// Licensed under the MIT License. See LICENSE file for details.


#include "stdlib.h"
#include "string.h"
#include "assert.h"

#include "../src/microbus.h"
#include "../src/networkManager.h"

uint32_t statsNodeJoinedNw = 0;
uint32_t networkFullCount = 0;

void test_new_node_given_id(void) {
    // Master
    tPacket masterPacket = {0};
    tNetworkManager nwManager = {0};
    uint8_t nodeTTL[10] = {0};
    // Node
    tPacket nodePacket = {0};
    uint64_t uniqueId = 7;
    tNodeIndex nodeId = INVALID_NODE_ID;
    int32_t timeToLive;

    tNodeQueue activeNodes = {0};
    tNodeQueue activeTxNodes = {0};
    networkManagerInit(&nwManager, &activeNodes);
    
    txNewNodeRequest(&nodePacket, uniqueId);
    rxNewNodePacketRequest(&nwManager, nodeTTL, &nodePacket, &networkFullCount);
    txNewNodeResponse(&nwManager, &masterPacket);
    rxNewNodePacketResponse(&masterPacket, uniqueId, &nodeId, &timeToLive, &statsNodeJoinedNw);
    
    assert(FIRST_NODE_ID == nodeId);
}

// void test_node_removed_from_network(void) {
//     // Master
//     tPacket masterPacket = {0};
//     tNetworkManager nwManager = {0};
//     uint8_t nodeTTL[10] = {0};

//     tNodeIndex activeNodeIds[MAX_NODES];
//     tNodeQueue activeNodes = {0};
//     tNodeIndex activeTxNodeIds[MAX_NODES];
//     tNodeQueue activeTxNodes = {0};
//     networkManagerInit(&nwManager, &activeNodes);
    
//     tNodeIndex nodeId = 1;
//     nodeTTL[nodeId] = MAX_ALLOCATED_SLOTS_TO_LIVE;
//     nodeQueueAdd(&activeNodes, nodeId);

//     for (uint32_t i=0; i<MAX_ALLOCATED_SLOTS_TO_LIVE; i++) {
//         decrementSlotsToLive(&nwManager, nodeTTL, nodeId);
//     }
    
//     // Check it's TTL is 0
//     assert(0 == nodeTTL[nodeId]);
// }

// void test_node_not_removed_from_network(void) {
//     // Master
//     tPacket masterPacket = {0};
//     tNetworkManager nwManager = {0};
//     uint8_t nodeTTL[10] = {0};

//     tNodeIndex activeNodeIds[MAX_NODES];
//     tNodeQueue activeNodes = {0};
//     tNodeIndex activeTxNodeIds[MAX_NODES];
//     tNodeQueue activeTxNodes = {0};
//     networkManagerInit(&nwManager, &activeNodes);
    
//     tNodeIndex nodeId = 1;
//     nodeTTL[nodeId] = MAX_ALLOCATED_SLOTS_TO_LIVE;
//     nodeQueueAdd(&activeNodes, nodeId);
    
//     for (uint32_t i=0; i<MAX_ALLOCATED_SLOTS_TO_LIVE+1; i++) {
//         if (i == MAX_ALLOCATED_SLOTS_TO_LIVE-1) {
//             networkManagerRecordRxPacket(&nwManager, nodeTTL, nodeId);
//         }
//         decrementSlotsToLive(&nwManager, nodeTTL, nodeId);
//     }
    
//     // Check the TTL has been reset
//     assert(MAX_ALLOCATED_SLOTS_TO_LIVE-2 == nodeTTL[nodeId]);
    
// }

void testNetworkManager() {
    test_new_node_given_id();
    // test_node_removed_from_network();
    // test_node_not_removed_from_network();
}



