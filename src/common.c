#include "string.h"
#include "stdbool.h"
#include "stdint.h"
#include "stdio.h"
#include "stdlib.h"
//#include "scheduler.h"
#include "microbus.h"
#include "txManager.h"
#include "networkManager.h"


// ========================================= //
// Process Rx

inline static bool checkValidProtocolVersion(tNode * node, tPacket * packet) {
    uint8_t protocolVersion = (packet->protocolVersionAndPacketType >> 4) & 0xF;
    if (packet->protocolVersionAndPacketType == 0xFF) {
        // 0xFF means there is probably nothing sent so return
        return false;
    } else if (protocolVersion != MICROBUS_VERSION) {
        myAssert(0, "Invalid version");
        // TODO: increment counter
        return false;
    }
    return true;
}

inline static uint8_t calcMasterPacketCheckSum(tPacket * packet) {
    uint8_t checkSum = packet->protocolVersionAndPacketType + packet->master.dstNodeId;
    for (uint8_t i=0; i<NUM_TX_NODES_SCHEDULED; i++) {
        checkSum += packet->master.nextTxNodeId[i];
        checkSum += packet->master.nextTxNodeAckSeqNum[i];
    }
    return checkSum;
}

inline static tPacket * processRxDataPacket(tNode * node, tPacket * packet, uint8_t srcNodeId) {
    // If it's the sequence number is expected
    if(rxPacketCheckAndUpdateSeqNum(&node->txManager, srcNodeId, packet->txSeqNum)) {
        if (CIRCULAR_BUFFER_FULL(node->rxPacketsStart, node->rxPacketsEnd, node->maxRxPackets)) {
            myAssert(0, "Rx packet buffer full!");
            // If full re-use the previous rx packet so that we can still update our meta-data from the rx headers
            return packet;
        } else {
            // Leave in the buffer to be processed outside of this driver
            // Update the tail and use the next free entry
            node->rxPacketsEnd = INCR_AND_WRAP(node->rxPacketsEnd, 1, node->maxRxPackets);
            return &node->rxPackets[node->rxPacketsEnd];
        }
    }
    return packet;
}

// Return pointer to the buffer entry for the next rx packet
// NOTE: this can't take too long. It must complete before the next packet is transmitted
inline static tPacket * masterProcessRx(tNode * node, tPacket * packet) {
    // By default re-use the rx packet for the next rx as well
    tPacket * nextRxPacket = packet;
    tPacketType packetType = packet->protocolVersionAndPacketType & 0xF;
    
    if (checkValidProtocolVersion(node, packet) == false) {
        return nextRxPacket;
    }
    
    bool crcValid = true;
    if (!crcValid) {
        // TODO: increment counter
        myAssert(0, "Invalid CRC");
    } else {
        if (packetType == NODE_DATA_PACKET) {
            recordPacketRecieved(&node->nwManager, node->nodeTTL, node->maxRxNodes, packet->node.srcNodeId);
            nextRxPacket = processRxDataPacket(node, packet, packet->node.srcNodeId);
            
        } else if (packetType == NEW_NODE_REQUEST_PACKET) {
            rxNewNodePacketRequest(node, packet);
        } else {
            myAssert("Invalid packet type");
            // TODO: increment counter
        }
    }
    return nextRxPacket;
}

// Return pointer to the buffer entry for the next rx packet
// NOTE: this can't take too long. It must complete before the next packet is transmitted
inline static tPacket * nodeProcessRx(tNode * node, tPacket * packet) {
    // By default re-use the rx packet for the next rx as well
    tPacket * nextRxPacket = packet;
    tPacketType packetType = packet->protocolVersionAndPacketType & 0xF;
    
    if (checkValidProtocolVersion(node, packet) == false) {
        return nextRxPacket;
    }
    
    // Record the master schedule regardless of whether they are the dst node or not
    if (packetType == MASTER_DATA_PACKET) {
        // We don't want to have to CRC the whole packet just for the schedule bytes
        // so we use a check sum instead
        if (packet->master.checkSum == calcMasterPacketCheckSum(packet)) {
            // Record scheduling info
            for (uint8_t i=0; i<NUM_TX_NODES_SCHEDULED; i++) {
                node->nextTxNodeId[i] = packet->master.nextTxNodeId[i];
            }
        }
    }
    
    // Only process the rest of the packet if it's for us
    if (packet->master.dstNodeId == node->nodeId) {
        bool crcValid = true;
        if (!crcValid) {
            // TODO: increment counter
            myAssert(0, "Invalid CRC");
        } else {
            if (packetType == MASTER_DATA_PACKET) {
                nextRxPacket = processRxDataPacket(node, packet);
                
            } else if (packetType == NEW_NODE_RESPONSE_PACKET && !node->isMaster) {
                if (node->nodeId == UNALLOCATED_NODE_ID) {
                    rxNewNodePacketResponse(node, packet);
                }
            }
        } else {
            myAssert("Invalid packet type");
            // TODO: increment counter
        }
    }
    return nextRxPacket;
}

// ========================================= //
// Update Schedule

inline static void shiftNextTxNodeSchedule(tNode * node) {
    // Shift schedule for next slot
    for (uint8_t i=0; i<NUM_TX_NODES_SCHEDULED-1; i++) {
        node->nextTxNodeId[i] = node->nextTxNodeId[i+1];
    }
    node->nextTxNodeId[NUM_TX_NODES_SCHEDULED-1] = INVALID_NODE_ID;
}

inline static void masterUpdateSchedule(tNode * node) {
    // Record that each of these nodes have been given a chance to Tx
    // If they don't use it often enough they will be assumed to have disconnected
    // and will be removed
    if (node->nextTxNodeId[0] != MASTER_NODE_ID) {
        recordNodeTxChance(node, node->nextTxNodeId[0]);
    }
    shiftNextTxNodeSchedule(node);
    
    // Create next schedule
    if (DUAL_CHANNEL_MODE || node->nextTxNodeId[0] == MASTER_NODE_ID) {
        schedulerUpdateAndCalcNextTxNodes(node->scheduler, node->nodeTTL, node->maxRxNodes, node->nextTxNodeId);
    }
}

inline static void nodeUpdateSchedule(tNode * node) {
    if (node->nextTxNodeId[0] == node->nodeId) {
        node->ourTTL--;
    }
    shiftNextTxNodeSchedule(node);
}

// ========================================= //
// Process Tx

inline static tPacket * masterProcessTx(tNode * node, tPacket * rxPacket) {
    tPacket * txPacket = NULL;
    if (node->nextTxNodeId[0] == node->nodeId || DUAL_CHANNEL_MODE) {
        // Always respond to new node requests before sending normal data packets
        if (node->nwManager->numNewNodes > 0) {
            txPacket = txNewNodeResponse(&node);
        } else {
            txPacket = getNextTxPacket(&node->txManager, node->nextTxNodeId);
        }
        // Fill in schedule
        for (uint8_t i=0; i<NUM_TX_NODES_SCHEDULED; i++) {
            txPacket->master.nextTxNodeId[i] = node->nextTxNodeId[i];
        }
        // Checksum
        txPacket->master.checkSum = calcMasterPacketCheckSum(txPacket)
        // TODO: CRC
    }
    return txPacket;
}

inline static tPacket * nodeProcessTx(tNode * node, tPacket * rxPacket) {
    tPacket * txPacket = NULL;
    if (node->nextTxNodeId[0] == node->nodeId) {
        // If we haven't got a node ID then we can send a request when the next slot is unused
        if (node->nodeId == UNALLOCATED_NODE_ID) {
            txPacket = txNewNodeRequest(node);
        } else {
            // Optimisation - this ensures every node gets an ack back from the master before it transmits
            // so the acks always match - even if the master is always transmitting to node 1 and node 2 is
            // always transmitting to master
            rxAckSeqNum(&node->txManager, 0, rxPacket->ackSeqNum);
            // If we've got a node ID and it's our turn we can transmit
            txPacket = getNextTxPacket(&node->txManager, NULL);
            
            // If our TTL is low then we need to send a packet to remain on the network
            if (txPacket == NULL && node->ourTTL < 3) {
                txPacket = getTxEmptyPacket(node);
            }
            
            if (txPacket != NULL) {
                txPacket->node.bufferLevel = getNumInTxBuffer(node->txManager, MASTER_NODE_ID);
            }
        }
        
        // TODO: CRC
    }
    return txPacket;
}


// ========================================= //

void masterProcessRxAndGetTx(tNode * node, tPacket ** txPacket, tPacket ** nextRxPacket) {
    tPacket * rxPacket = &node->rxPackets[node->rxPacketsEnd];
    *nextRxPacket = masterProcessRx(node, rxPacket);
    masterUpdateScheduler(node);
    *txPacket = masterProcessTx(node, rxPacket);
    (*nextRxPacket)->protocolVersionAndPacketType = 0;
}

void processRxAndGetTx(tNode * node, tPacket ** txPacket, tPacket ** nextRxPacket) {
    tPacket * rxPacket = &node->rxPackets[node->rxPacketsEnd];
    *nextRxPacket = nodeProcessRx(node, rxPacket);
    nodeUpdateScheduler(node);
    *txPacket = nodeProcessTx(node, rxPacket);
    (*nextRxPacket)->protocolVersionAndPacketType = 0;
}

// ========================================= //
// Called by main thread

void nodeInit(tNode * node, uint8_t maxTxPacketEntries, tTxPacketEntry txPacketEntries[], uint8_t maxRxPackets, tPacket rxPackets[]) {
    memset(node, 0, sizeof(tNode));
    initTxManager(&node->txManager, MAX_TX_NODES, node->txSeqNumStart, node->txSeqNumEnd, node->txSeqNumNext, node->rxSeqNum, maxTxPacketEntries, txPacketEntries);
    //node->nodeId = 0; // TODO: node NODE ID
    node->maxRxPackets = maxRxPackets;
    node->rxPackets = rxPackets;
}

tMasterPacket * masterAllocateTxPacket(tNode * node, tNodeIndex dstNodeId) {
    tPacket * packet = allocateTxPacket(&node->txManager, node->nodeId, dstNodeId);
    return &packet->master;
}

tNodePacket * nodeAllocateTxPacket(tNode * node) {
    tPacket * packet = allocateTxPacket(&node->txManager, node->nodeId, MASTER_NODE_ID);
    return &packet->node;
}

void masterSubmitAllocatedTxPacket(tNode * node, tNodeIndex dstNodeId, uint16_t numBytes) {
    if (numBytes > MASTER_PACKET_DATA_SIZE) {
        myAssert(0, "Tx packet exceeds max size");
        return NULL;
    }
    tPacket * packet = node->txManager.allocatedPacket;
    if (packet != NULL) {
        packet->protocolVersionAndPacketType = (MICROBUS_VERSION << 4) | MASTER_DATA_PACKET;
        packet->dataSize1 = numBytes >> 8;
        packet->dataSize2 = numBytes & 0xFF;
        packet->master.dstNodeId = dstNodeId;
    }
    submitAllocatedTxPacket(node, dstNodeId);
}

void nodeSubmitAllocatedTxPacket(tNode * node, uint16_t numBytes) {
    if (numBytes > NODE_PACKET_DATA_SIZE) {
        myAssert(0, "Tx packet exceeds max size");
        return NULL;
    }
    tPacket * packet = node->txManager.allocatedPacket;
    if (packet != NULL) {
        packet->dataSize1 = numBytes >> 8;
        packet->dataSize2 = numBytes & 0xFF;
        packet->node.srcNodeId = node->nodeId;
    }
    submitAllocatedTxPacket(node, MASTER_NODE_ID);
}

tPacket * peekNextRxDataPacket(tNode * node) {
    tPacket * entry;
    CIRCULAR_BUFFER_PEEK(entry, node->rxPackets, node->rxPacketsStart, node->rxPacketsEnd, node->maxRxPackets);
    return entry;
}

void popNextDataPacket(tNode * node) {
    CIRCULAR_BUFFER_POP(node->rxPacketsStart, node->rxPacketsEnd, node->maxRxPackets);
}

