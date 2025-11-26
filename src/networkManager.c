// Copyright (c) 2025 Sean Bremner
// Licensed under the MIT License. See LICENSE file for details.

#include "string.h"
#include "stdbool.h"
#include "stdint.h"
#include "stdlib.h"

#include "microbus.h"
#include "networkManager.h"

// =============================================================== //
//                        Network Manager
// 
// This manages which nodes are accepted on to the network and the
// process to join. The basic idea behind joining is that the master
// advertises an joining slot occassionally and any node that wants to 
// join can send their unique ID in a random sub-slot in the packet.
// If too many try they will back-off. The master will then respond
// with response packet assigning nodeIds to uniqueIds. Once a node
// has a node ID it is on the network.
//
// Each node then has a TTL (time to live) that get decremented every
// time it is allocated a tx slot. The TTL is reset every time the node
// transmits a packet. If the TTL gets close to zero the node will need
// to send an empty packet just to keep the it's place on the network
// otherwise the master will assume it has disconnected and remove it.
//
// =============================================================== //

// Node

void nodeNwRecordTxPacketSent(int32_t * timeToLive) {
    *timeToLive = NODE_TIMEOUT_US;
}

// Called by timer thread
void nodeNwUpdateTimeUs(int32_t * timeToLive, uint32_t usIncr) {
    if (*timeToLive > 0) {
        (*timeToLive) -= usIncr;
    }
}

bool nodeNwHasNodeTimedOut(int32_t timeToLive) {
    return (timeToLive <= 0);
}


// ======================================== //
// Master

#define NEW_NODE_RESPONSE_ENTRY_SIZE 9 // uint64_t uniqueId; uint8_t nodeId;
#define NEW_NODE_REQUEST_ENTRY_SIZE 10 // uint64_t uniqueId, uint16_t checkSum

static tNodeIndex getNextFreeNodeId(uint8_t masterNodeTimeToLive[MAX_NODES]) {
    for (uint32_t nodeId = FIRST_NODE_ID; nodeId<MAX_NODES; nodeId++) {
        if (masterNodeTimeToLive[nodeId] == 0) {
            return nodeId;
        }
    }
    return INVALID_NODE_ID;
}

// Master - Check to see if we are waiting for a response from this uniqueID, 
// if not assign it a free node ID that will be transmitted later
void networkManagerRegisterNewNode(tNetworkManager * nwManager, uint8_t masterNodeTimeToLive[MAX_NODES], uint64_t uniqueId, uint32_t * networkFullCount) {
    microbusAssert(uniqueId != 0, "");

    // Check if there is space in our new nodes allocation queue
    if (nwManager->numNewNodes == MAX_NODES_ALLOCATED_AT_ONCE) {
        //microbusAssert(0, "No free new node spaces"); // TODO: info
        return;
    }

    // Check if this node has already been registered
    for (uint8_t i=0; i<nwManager->numNewNodes; i++) {
        if (nwManager->newNodeUniqueId[i] == uniqueId) {
            return;
        }
    }

    // Find free node ID
    // Start at last used - *this is important* - prevents nodes that think they have a node spot continuously getting packets
    uint8_t nodeId = getNextFreeNodeId(masterNodeTimeToLive);
    if(nodeId == INVALID_NODE_ID) {
        (*networkFullCount)++;
        return;
    }

    MB_NETWORK_MANAGER_PRINTF("Master - Node:%u partial join - uniqueId:0x%llx\n", nodeId, uniqueId);
    masterNodeTimeToLive[nodeId] = MASTER_MAX_TIME_TO_LIVE; // TODO: need to bring this down buy scheduling newly join nodes to tx as priority
    uint32_t index = nwManager->numNewNodes;
    nwManager->newNodeUniqueId[index] = uniqueId;
    nwManager->newNodeId[index] = nodeId;
    nwManager->numNewNodes++;
    nodeQueueAdd(nwManager->activeNodes, nodeId);
}

bool networkManagerRemoveNewNodeRequest(tNetworkManager * nwManager, tNodeIndex rxNodeId) {
    if (nwManager->numNewNodes > 0) {
        for (uint8_t i=0; i<nwManager->numNewNodes; i++) {
            if (nwManager->newNodeId[i] == rxNodeId) {
                microbusAssert(nwManager->newNodeUniqueId[i] > 0, "");
                // Node allocation complete - remove from new node table
                for (uint8_t j=i; j<nwManager->numNewNodes-1; j++) {
                    nwManager->newNodeUniqueId[j] = nwManager->newNodeUniqueId[j+1];
                    nwManager->newNodeId[j]       = nwManager->newNodeId[j+1];
                }
                nwManager->numNewNodes--;
                return true;
            }
        }
    }
    return false;
}

void networkManagerRecordRxPacket(tNetworkManager * nwManager, uint8_t masterNodeTimeToLive[MAX_NODES], tNodeIndex rxNodeId) {
    microbusAssert(rxNodeId < MAX_NODES, "");
    // Received data from this node - so TTL set to max
    masterNodeTimeToLive[rxNodeId] = MASTER_MAX_TIME_TO_LIVE;
    // MB_NETWORK_MANAGER_PRINTF("Master - Node:%u heard, TTL reset:%u\n", rxNodeId, masterNodeTimeToLive[rxNodeId]);

    // If a node we've recently given a nodeId to starts transmitting then it's heard our reponse and 
    // joined the network so we can remove it from our allocation table
    if (nwManager->numNewNodes > 0) {
        bool found = networkManagerRemoveNewNodeRequest(nwManager, rxNodeId);
        if (found) {
            MB_PRINTF("Node:%u - fully joined\n", rxNodeId);
        }
    }
}

// Called by timer thread
void networkManagerUpdateTimeUs(tNetworkManager * nwManager, uint8_t masterNodeTimeToLive[MAX_NODES], uint32_t usIncr) {
    // Each node must transmit within a certain time or be removed

    // Increment a global timer
    nwManager->timeToLiveTimeUs += usIncr;

    // When global timer is above a certain level update all counts
    if (nwManager->timeToLiveTimeUs >= TIME_TO_LIVE_UPDATE_TIME_US) {
        nwManager->timeToLiveTimeUs -= TIME_TO_LIVE_UPDATE_TIME_US;
        
        for (uint32_t nodeId=FIRST_NODE_ID; nodeId<MAX_NODES; nodeId++) {
            if (masterNodeTimeToLive[nodeId] > 0 && masterNodeTimeToLive[nodeId] != REMOVE_NODE_TTL) {
                masterNodeTimeToLive[nodeId]--;
                if (masterNodeTimeToLive[nodeId] == 0) {
                    // Mark it as needing to be removed
                    masterNodeTimeToLive[nodeId] = REMOVE_NODE_TTL;
                }
            }
        }
    }
}

// ======================================== //
// Packets

// Node - Ask for a node ID
tPacket * txNewNodeRequest(tPacket * packet, uint64_t uniqueId) {
    // TODO: this needs a backoff!
    
    SET_PROTOCOL_VERSION_AND_PACKET_TYPE(packet, NEW_NODE_REQUEST_PACKET);
    packet->node.srcNodeId = UNALLOCATED_NODE_ID;
    packet->dataSize1 = 0;
    packet->dataSize2 = 9;
    memcpy(&packet->node.data[0], &uniqueId, 8);
    return packet;
}

// Master
void rxNewNodePacketRequest(tNetworkManager * nwManager, uint8_t masterNodeTimeToLive[MAX_NODES], tPacket * packet, uint32_t * networkFullCount) {
    uint64_t uniqueId;
    microbusAssert(packet->dataSize2 == 9, "");
    memcpy(&uniqueId, &packet->node.data[0], 8);
    networkManagerRegisterNewNode(nwManager, masterNodeTimeToLive, uniqueId, networkFullCount);
}

// Master
void txNewNodeResponse(tNetworkManager * nwManager, tPacket * packet) {
    SET_PROTOCOL_VERSION_AND_PACKET_TYPE(packet, NEW_NODE_RESPONSE_PACKET);
    packet->master.dstNodeId = UNALLOCATED_NODE_ID;

    MB_NETWORK_MANAGER_PRINTF("%s", "Master, Tx new node response:");
    
    uint8_t maxNum = MIN(nwManager->numNewNodes, MASTER_PACKET_DATA_SIZE / NEW_NODE_RESPONSE_ENTRY_SIZE);
    uint8_t num = 0;
    for (uint8_t i=0; i<MAX_NODES_ALLOCATED_AT_ONCE; i++) {
        if (nwManager->newNodeUniqueId[i] > 0) {
            uint64_t uniqueId = nwManager->newNodeUniqueId[i];
            uint8_t nodeId = nwManager->newNodeId[i];
            MB_NETWORK_MANAGER_PRINTF_WITHOUT_NEW_LINE(" %u:0x%llx, ", nodeId, uniqueId);
            memcpy(&packet->master.data[num*NEW_NODE_RESPONSE_ENTRY_SIZE], &uniqueId, 8);
            packet->master.data[8 + num*NEW_NODE_RESPONSE_ENTRY_SIZE] = nodeId;
            num++;
            if (num == maxNum) {
                break;
            }
        }
    }
    
    MB_NETWORK_MANAGER_PRINTF_WITHOUT_NEW_LINE("%s", "\n");

    
    uint16_t dataSize = num * NEW_NODE_RESPONSE_ENTRY_SIZE;
    SET_PACKET_DATA_SIZE(packet, dataSize);
}

// Node - Process the new node response - return the node ID if we've been allocated one
void rxNewNodePacketResponse(tPacket * packet, uint64_t uniqueId, tNodeIndex * nodeId, int32_t * timeToLive, uint32_t * statsNodeJoined) {
    uint16_t dataSize = GET_PACKET_DATA_SIZE(packet);
    uint8_t numEntries = dataSize / NEW_NODE_RESPONSE_ENTRY_SIZE;

    for (uint8_t i=0; i<numEntries; i++) {
        uint64_t rxUniqueId;
        memcpy(&rxUniqueId, &packet->master.data[i*NEW_NODE_RESPONSE_ENTRY_SIZE], 8);
        uint8_t tmpNodeId = packet->master.data[i*NEW_NODE_RESPONSE_ENTRY_SIZE + 8];
        if (rxUniqueId == uniqueId) {
            *nodeId = tmpNodeId;
            (*statsNodeJoined)++;
            *timeToLive = NODE_TIMEOUT_US;
            MB_PRINTF("Node:%u - joined - uniqueId:0x%llx\n", tmpNodeId, uniqueId);
            return;
        }
    }
}

// ==================================================================== //

void networkManagerInit(tNetworkManager * nwManager, tNodeQueue * activeNodes) {
    nwManager->activeNodes = activeNodes;
}

