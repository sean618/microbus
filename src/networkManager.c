#include "string.h"
#include "stdbool.h"
#include "stdint.h"
#include "stdio.h"
#include "stdlib.h"
#include "usefulLib.h"
#include "microbus.h"
#include "networkManager.h"
#include "txManager.h"

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


#define NEW_NODE_RESPONSE_ENTRY_SIZE 9 // uint64_t uniqueId; uint8_t nodeId;
#define NEW_NODE_REQUEST_ENTRY_SIZE 10 // uint64_t uniqueId, uint16_t checkSum

// Master - Check to see if we are waiting for a response from this uniqueID, 
// if not assign it a free node ID that will be transmitted later
static void registerNewNode(tNetworkManager * nwManager, uint8_t * nodeTTL, uint8_t maxRxNodes, uint64_t uniqueId) {
    for (uint8_t i=0; i<MAX_NODES_ALLOCATED_AT_ONCE; i++) {
        if (nwManager->newNodeUniqueId[i] == uniqueId) {
            // Already exists
            return;
        }
    }
    for (uint8_t i=0; i<MAX_NODES_ALLOCATED_AT_ONCE; i++) {
        if (nwManager->newNodeUniqueId[i] == 0) {
            // Find free node ID
            for (uint8_t nodeId=FIRST_AGENT_NODE_ID; nodeId<maxRxNodes; nodeId++) {
                if (nodeTTL[nodeId] == 0) {
                    // Give it 5 turns to transmit (if it hasn't sent anything by then it will get removed)
                    nodeTTL[nodeId] = 5;
                    nwManager->newNodeUniqueId[i] = uniqueId;
                    nwManager->newNodeId[i] = nodeId;
                    nwManager->numNewNodes++;
                    return;
                }
            }
            myAssert(0, "Max node IDs reached"); // TODO: info
            return;
        }
    }
    myAssert(0, "No free new node spaces"); // TODO: info
}

void recordPacketRecieved(tNetworkManager * nwManager, uint8_t * nodeTTL, uint8_t maxRxNodes, tNodeIndex rxNodeId) {
    myAssert(rxNodeId < maxRxNodes, "");
    // Received data from this node - so TTL set to max
    nodeTTL[rxNodeId] = MAX_SLOTS_TILL_RX_REQUIRED;
    
    // If a node we've recently given a nodeId to starts transmitting then it's heard our reponse and 
    // joined the network so we can remove it from our allocation table
    if (nwManager->numNewNodes > 0) {
        for (uint8_t i=0; i<MAX_NODES_ALLOCATED_AT_ONCE; i++) {
            if (nwManager->newNodeId[i] == rxNodeId) {
                // Node allocation complete - remove from new node table
                nwManager->newNodeUniqueId[i] = 0;
                nwManager->numNewNodes--;
            }
        }
    }
}

void recordNodeTxChance(uint8_t * nodeTTL, uint8_t maxRxNodes, tNodeIndex rxNodeId, tTxManager * txManager, uint8_t * rxPacketsStart, uint8_t * rxPacketsEnd) {
    myAssert(rxNodeId < maxRxNodes, "");
    myAssert(nodeTTL[rxNodeId] > 0, "Node scheduled but TTL is 0?!");
    // This node had a slot to transmit decrement it's TTL
    // If it reaches 0 remove it from the network
    nodeTTL[rxNodeId]--;
    if (nodeTTL[rxNodeId] == 0) {
        // Clear all tx packets
        txManagerRemoveNode(txManager, rxNodeId);
        // Clear all rx packets
        *rxPacketsStart = *rxPacketsEnd;
    }
}

// ======================================== //
// Packets

static uint16_t calculateNodeRequestChecksum(uint64_t uniqueId) {
    return (0xFFFF & (uniqueId >> 48)) 
            + (0xFFFF & (uniqueId >> 32)) 
            + (0xFFFF & (uniqueId >> 16)) 
            + (0xFFFF & (uniqueId));
}

// Agent - Ask for a node ID
tPacket * txNewNodeRequest(tPacket * packet, uint64_t uniqueId) {
    // TODO: this needs a backoff!
    //tPacket * packet = &node->tmpPacket;
    packet->protocolVersionAndPacketType = (MICROBUS_VERSION & 0xF) << 4 | (uint8_t) NEW_NODE_REQUEST_PACKET;
    packet->node.srcNodeId = INVALID_NODE_ID;
    // Set the entire packet to be floating/pulled-up
    memset(packet->node.data, 0xFF, NODE_PACKET_DATA_SIZE);
    // Choose a random offset into the packet (aligned to boundaries)
    uint8_t offset = rand() % (NODE_PACKET_DATA_SIZE / NEW_NODE_REQUEST_ENTRY_SIZE);
    uint32_t byteOffset = 1 + (offset * NEW_NODE_REQUEST_ENTRY_SIZE);
    uint16_t checkSum = calculateNodeRequestChecksum(uniqueId);
    memcpy(&packet->node.data[byteOffset],   &uniqueId, 8);
    memcpy(&packet->node.data[byteOffset+8], &checkSum, 2);
    //MB_PRINTF(node, "Sending new node packet: byteoffset: %u, %lu\n", byteOffset, node->uniqueId);
    return packet;
}

// Master
void rxNewNodePacketRequest(tNetworkManager * nwManager, uint8_t * nodeTTL, uint8_t maxRxNodes, tPacket * packet) {
    for (uint32_t i=0; i<MASTER_PACKET_DATA_SIZE; i+=NEW_NODE_REQUEST_ENTRY_SIZE) {
        uint64_t newNodeUniqueId;
        uint16_t rxCheckSum;
        memcpy(&newNodeUniqueId, &packet->node.data[1+i],   8);
        memcpy(&rxCheckSum,      &packet->node.data[1+i+8], 2);
        uint16_t checkSum = calculateNodeRequestChecksum(newNodeUniqueId);
        if (checkSum == rxCheckSum) {
            registerNewNode(nwManager, nodeTTL, maxRxNodes, newNodeUniqueId);
        }
    }
}

// Master
tPacket * txNewNodeResponse(tNetworkManager * nwManager, tPacket * packet) {
    //tPacket * packet = &node->tmpPacket;
    packet->protocolVersionAndPacketType = (MICROBUS_VERSION & 0xF) << 4 | (uint8_t) NEW_NODE_RESPONSE_PACKET;
    packet->master.dstNodeId = UNALLOCATED_NODE_ID;
    
    uint8_t maxNum = MIN(nwManager->numNewNodes, MASTER_PACKET_DATA_SIZE / NEW_NODE_RESPONSE_ENTRY_SIZE);
    uint8_t num = 0;
    for (uint8_t i=0; i<MAX_NODES_ALLOCATED_AT_ONCE; i++) {
        if (nwManager->newNodeUniqueId[i] > 0) {
            uint64_t uniqueId = nwManager->newNodeUniqueId[i];
            uint8_t nodeId = nwManager->newNodeId[i];
            memcpy(&packet->master.data[num*NEW_NODE_RESPONSE_ENTRY_SIZE], &uniqueId, 8);
            packet->master.data[8 + num*NEW_NODE_RESPONSE_ENTRY_SIZE] = nodeId;
            num++;
            if (num == maxNum) {
                break;
            }
        }
    }
    
    uint16_t dataSize = num * NEW_NODE_RESPONSE_ENTRY_SIZE;
    packet->dataSize1 = dataSize >> 8;
    packet->dataSize2 = dataSize & 0xFF;
    return packet;
}

// Agent - Process the new node response
void rxNewNodePacketResponse(tPacket * packet, uint64_t uniqueId, uint8_t * nodeId) {
    uint16_t dataSize = packet->dataSize1 << 8 | packet->dataSize2;
    uint8_t numEntries = dataSize / NEW_NODE_RESPONSE_ENTRY_SIZE;
    for (uint8_t i=0; i<numEntries; i++) {
        uint64_t rxUniqueId;
        memcpy(&rxUniqueId, &packet->master.data[i*NEW_NODE_RESPONSE_ENTRY_SIZE], 8);
        if (rxUniqueId == uniqueId) {
            *nodeId = packet->master.data[i*NEW_NODE_RESPONSE_ENTRY_SIZE + 8];
            printf("Allocated a node ID:%u\n", *nodeId);
            return;
        }
    }
}

