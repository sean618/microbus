// Copyright (c) 2025 Sean Bremner
// Licensed under the MIT License. See LICENSE file for details.

#include "string.h"
#include "stdbool.h"
#include "stdint.h"
#include "stdlib.h"

//#include "scheduler.h"
#include "microbus.h"
#include "node.h"
#include "txManager.h"
#include "rxManager.h"
#include "networkManager.h"

static void nodeRemoveFromNetwork(tNode * node);

static uint32_t seed = 1;

static void customSrand(uint32_t s) {
    seed = s ? s : 1;  // Ensure seed is non-zero
}

static int customRand(void) {
    seed = seed * 1103515245 + 12345;  // Example linear congruential generator
    return (seed >> 16) & 0x7FFF;
}

// ==================================================================== //

void nodeQuickProcessPrevRx(tNode * node, bool rxCrcError) {
    if (!node->initialised) {
        return;
    }
    node->prevRxPacketEntry = node->nextRxPacketEntry;
    tPacket * rxPacket = &node->prevRxPacketEntry->packet;
    node->validRxPacket = false;
    node->savedRxAckValid = false;

    // New node requests have their own CRC as multiple nodes can transmit in that packet slot
    if (rxCrcError) {
        node->stats.rxCrcFailures++;
        return;
    }

    // If the version and packet type is 0 then it's probably just an empty packet
    if (rxPacket->protocolVersionAndPacketType == 0 || rxPacket->protocolVersionAndPacketType == 255) {
        return;
    }

    if (GET_PROTOCOL_VERSION(rxPacket) != MICROBUS_VERSION) {
        node->stats.rxInvalidProtocol++;
        return;
    }

    if (GET_PACKET_TYPE(rxPacket) == MASTER_RESET_PACKET) {
        nodeRemoveFromNetwork(node);
        return;
    }

    // Record the master schedule - even if the packet is not for us
    for (uint8_t i=0; i<MAX_TX_NODES_SCHEDULED; i++) {
        node->nextTxNodeId[i] = rxPacket->master.nextTxNodeId[i];
    }

    if (node->nodeId != UNALLOCATED_NODE_ID) {
        // Record the acks for us - even if the packet is not for us
        for (uint8_t i=0; i<MAX_TX_NODES_SCHEDULED; i++) {
            if (rxPacket->master.nextTxNodeId[i] == node->nodeId) {
                node->savedRxAckValid = true;
                node->savedRxAck = rxPacket->master.nextTxNodeAckSeqNum[i];
            }
        }
    }

    // Check if it's for us
    if (rxPacket->master.dstNodeId != node->nodeId) {
        return;
    }

    node->validRxPacket = true;
    node->stats.rxValid++;
    
    // NOTE: by default we will reuse the current rx packet (unless there is valid rx data)
    // We will store this packet so use a new one for the next rx packet
    tPacketEntry * freeEntry = findFreeRxPacket(&node->rxPacketManager);
    if (freeEntry) {
        node->nextRxPacketEntry = freeEntry;

        // We want to update our sequence number as quickly as possible so we can ack it straight away
        node->validRxSeqNum = true;
        if (node->nodeId != UNALLOCATED_NODE_ID) {
            if (GET_PACKET_TYPE(rxPacket) == MASTER_DATA_PACKET) {
                node->validRxSeqNum = rxPacketCheckAndUpdateSeqNum(&node->txManager, MASTER_NODE_ID, rxPacket->txSeqNum, false);
            }
        }
    } else {
        node->stats.rxBufferFull++;
        // Re-use this packet 
        node->validRxPacket = false;
    }
    microbusAssert(node->nextRxPacketEntry, "");
}

static void nodeProcessRxData(tNode * node, tPacketEntry * packetEntry) {
    tPacket * packet = &packetEntry->packet;
    tPacketType packetType = GET_PACKET_TYPE(packet);
    bool packetStored = false;
    node->stats.rxPacketEntries++;

    if (node->nodeId == UNALLOCATED_NODE_ID) {
        if (packetType == NEW_NODE_RESPONSE_PACKET) {
            if (node->sentNewNodeRequest) {
                node->stats.newNodeAllocatedRx++;
                rxNewNodePacketResponse(packet, node->uniqueId, &node->nodeId, &node->timeToLive, &node->stats.nodeJoinedNw);
            }
        }
    } else {
        // Only process the rest of the packet if it's for us
        if (packet->master.dstNodeId == node->nodeId) {
            switch (packetType) {
                case MASTER_DATA_PACKET:
                    microbusAssert(packet->dataSize1 > 0 || packet->dataSize2 > 0, "");
                    // if(rxPacketCheckAndUpdateSeqNum(&node->txManager, MASTER_NODE_ID, packet->txSeqNum, false)) {
                    if (node->validRxSeqNum) {
                        node->stats.rxDataPackets++;
                        addRxDataPacket(&node->rxPacketManager, packetEntry);
                        packetStored = true;
                    }
                    break;
                default:
                    node->stats.rxInvalidPacketType++;
                    break;
            }
        }
    }

    if (!packetStored) {
        // Free the packet
        packetEntry->inUse = false;
    }
}

// ========================================= //
// Update Schedule

void nodeUpdateSchedule(tNode * node) {
    // Shift down
    node->currentTxNodeId = node->nextTxNodeId[0];
    for (uint8_t i=0; i<MAX_TX_NODES_SCHEDULED-1; i++) {
        node->nextTxNodeId[i] = node->nextTxNodeId[i+1];
    }
    node->nextTxNodeId[MAX_TX_NODES_SCHEDULED-1] = INVALID_NODE_ID;

    // MB_PRINTF("Node:%u - schedule: %u,%u,%u,%u\n", node->nodeId, node->nextTxNodeId[0], node->nextTxNodeId[1], node->nextTxNodeId[2], node->nextTxNodeId[3]);
}

// ========================================= //
// Process Tx

static void nodeProcessTx(tNode * node) {
    tPacket * txPacket = NULL;
    // Prepare the next packet for when it's our turn

    // If we haven't sent the last packet (and that packet wasn't just an empty packet)
    // then keep trying (don't get the next one)
    if (node->nextTxPacket) {
        if ((GET_PACKET_TYPE(node->nextTxPacket) == NODE_DATA_PACKET)
             || (GET_PACKET_TYPE(node->nextTxPacket) == NEW_NODE_REQUEST_PACKET)) {
            return;
        }
    }

    if (node->nodeId != UNALLOCATED_NODE_ID) {
        // MB_NETWORK_MANAGER_PRINTF("Node:%u TTL reset:%u\n", node->nodeId, node->timeToLive);

        // If we've got a node ID and it's our turn we can transmit
        txPacket = nodeGetNextTxDataPacket(&node->txManager);

        if (txPacket == NULL) {
            // Alternate between 2 empty packet headers
            // This is because the packet memory is only DMA'd a cycle later
            node->tmpPacketCycle++;
            if (node->tmpPacketCycle >= 2) {
                node->tmpPacketCycle = 0;
            }

            // We always send a packet when it's our turn
            // Just send an empty one
            tPacketHeader * txPacketHeader = &node->tmpEmptyPacketHeader[node->tmpPacketCycle];
            SET_PROTOCOL_VERSION_AND_PACKET_TYPE(txPacketHeader, NODE_EMPTY_PACKET);
            SET_PACKET_DATA_SIZE(txPacketHeader, 0);
            txPacketHeader->node.srcNodeId = node->nodeId;
            txPacketHeader->txSeqNum = INVALID_SEQUENCE_NUM;
            txPacketHeader->node.ackSeqNum = node->txManager.rxSeqNum[MASTER_NODE_ID];
            txPacket = (tPacket *)txPacketHeader; // A bit hacky - the DMA will access a few hundred bytes beyond the packet header

            // MB_TX_MANAGER_PRINTF("Node:%u Prepare Tx Empty packet\n", node->nodeId);
        }
        
        if (txPacket != NULL) {
            txPacket->node.bufferLevel = getNumInTxBuffer(&node->txManager, MASTER_NODE_ID);
        }
    } else {
        // If we haven't got a node ID then we can send a request when the next slot is unused
        if (node->nextNewNodeResponseCountdown == 0) {
            node->stats.newNodeRequest++;
            node->sentNewNodeRequest = true;
            txPacket = txNewNodeRequest(&node->tmpPacket, node->uniqueId);
            node->nextNewNodeResponseCountdown = customRand() % MAX_NEW_NODE_BACKOFF;
            MB_NETWORK_MANAGER_PRINTF("Node %u, Prepare Tx new node request: 0x%llx, (backoff:%u)\n", node->nodeId, node->uniqueId, node->nextNewNodeResponseCountdown);
        } else {
            node->nextNewNodeResponseCountdown--;
            MB_PRINTF("New node request countdown:%u\n", node->nextNewNodeResponseCountdown);
        }
    }

    node->nextTxPacket = txPacket;
}


// ========================================= //


void nodeProcessRx(tNode * node) {
    if (node->savedRxAckValid) {
        rxAckSeqNum(&node->txManager, MASTER_NODE_ID, node->savedRxAck, false, &node->stats.txWindowRestarts);
    }
    if (node->validRxPacket) {
        nodeProcessRxData(node, node->prevRxPacketEntry); // Process the last rx packet
    } else {
        // Packet will be reused
    }
}


void nodeQuickUpdateTxPacket(tNode * node) {
    // Update the ack 
    if (node->nextTxPacket) {
        node->nextTxPacket->node.ackSeqNum = node->txManager.rxSeqNum[MASTER_NODE_ID];
    }
}

tPacket * nodeGetTxPacket(tNode * node) {
    // Update now that we've processed any rx schedule
    // if ((node->currentTxNodeId == node->nodeId) && (node->nextTxPacket != NULL)) {
    if (node->nextTxPacket != NULL) {
        // Make sure we only sent new node responses after a backoff
        if (node->nodeId == UNALLOCATED_NODE_ID) {
            if (node->nextNewNodeResponseCountdown == 0) {
                node->stats.newNodeRequest++;
                node->nextNewNodeResponseCountdown = customRand() % MAX_NEW_NODE_BACKOFF;
                MB_NETWORK_MANAGER_PRINTF("Node, Tx new node request: 0x%llx, (backoff:%u)\n", node->uniqueId, node->nextNewNodeResponseCountdown);
            } else {
                node->nextNewNodeResponseCountdown--;
                return NULL;
            }
        }

        // Now fill in the updated schedule and the acks
        nodeQuickUpdateTxPacket(node);
        tPacket * txPacket = node->nextTxPacket;
        node->nextTxPacket = NULL;

        // Record some stats
        node->stats.txPackets++;
        if (GET_PACKET_TYPE(txPacket) == NODE_DATA_PACKET) {
            node->stats.txDataPackets++;
        }
        if (MICROBUS_LOG_PACKETS && MICROBUS_LOGGING) {
            microbusPrintPacket(txPacket, false, node->nodeId, true, 0);
        }

        // Update our timeout as we've send something to the master
        if (txPacket) {
            nodeNwRecordTxPacketSent(&node->timeToLive);
        }
        return txPacket;
    }
    return NULL;
}

// ========================================= //

void nodeCheckIfTimedOut(tNode * node) {
    if (nodeNwHasNodeTimedOut(node->timeToLive)) {
        nodeRemoveFromNetwork(node);
    }
}

// ========================================= //
// Dual channel pipelined
// The pre process is quick before the slot starts packets go out
// The post process is slow and can be run whilst the next packet is 
// being transmitted and received. 
// This is for the main SPI link which needs to run as fast as possible

void nodeDualChannelPipelinedPreProcess(tNode * node, tPacket ** txPacket, tPacket ** rxPacketMemory, bool crcError) {
    if (!node->initialised) {
        return;
    }
    // Quick validate rx packet and record the seq nums so we can ack them as soon as possible
    nodeQuickProcessPrevRx(node, crcError);
    // Update the schedule after receiving the node schedule from the master 
    // That way we stay in sync - as the master technically sent it last turn
    nodeUpdateSchedule(node);
    *rxPacketMemory = &node->nextRxPacketEntry->packet;

    if ((node->currentTxNodeId == node->nodeId) && (node->nextTxPacket != NULL)) {
        *txPacket = nodeGetTxPacket(node);
    }
}

void nodeDualChannelPipelinedPostProcess(tNode * node) {
    if (!node->initialised) {
        return;
    }
    nodeCheckIfTimedOut(node);
    // Process the recieved frame
    nodeProcessRx(node);
    // Prepare the next tx frame
    nodeProcessTx(node);
}

// ========================================= //
// Single channel - no pipeline
// As it's single channel it's either Tx or Rx - so separate calls
// No pipeline so we can just process everything as it comes in or goes out

tPacket * nodeGetRxPacketMemory(tNode * node) {
    return &node->nextRxPacketEntry->packet;
}

bool nodeIsTxMode(tNode * node) {
    if (!node->initialised) {
        return false;
    }
    bool isTx = (node->currentTxNodeId == node->nodeId);
    nodeCheckIfTimedOut(node);
    nodeUpdateSchedule(node);
    return isTx;
}

void nodeNoDelaySingleChannelProcessRx(tNode * node, bool crcError) {
    if (!node->initialised) {
        return;
    }
    // nodeProcessTx(node);
    nodeQuickProcessPrevRx(node, crcError);
    nodeProcessRx(node);
}

void nodeNoDelaySingleChannelProcessTx(tNode * node, tPacket ** txPacket) {
    if (!node->initialised) {
        return;
    }
    nodeProcessTx(node);
    *txPacket = nodeGetTxPacket(node);
}

// ========================================= //

// Called by any thread
void nodeUpdateTimeUs(tNode * node, uint32_t usIncr) {
//    int32_t prevTime = node->timeToLive;
    nodeNwUpdateTimeUs(&node->timeToLive, usIncr);
    #if MICROBUS_LOGGING
        if (nodeNwHasNodeTimedOut(prevTime) == false && nodeNwHasNodeTimedOut(node->timeToLive)) {
            MB_PRINTF("Node:%u, timed out: %d\n", node->nodeId, node->timeToLive);
        }
    #endif
}

// ========================================= //
// Called by main thread

void nodeReset(tNode * node) {
    nodeInit(node, 
            node->uniqueId,
            node->txManager.packetStore.maxEntries, 
            node->txManager.packetStore.entries, 
            node->rxPacketManager.maxRxPacketEntries, 
            node->rxPacketManager.rxPacketEntries,
            node->rxPacketManager.rxPacketQueue);
}

static void nodeRemoveFromNetwork(tNode * node) {
    if (node->nodeId != UNALLOCATED_NODE_ID) {
        MB_PRINTF("Node:%u, left network, 0x%llx\n", node->nodeId, node->uniqueId);
        uint32_t nodeLeftNw = node->stats.nodeLeftNw;
        uint32_t nodeJoinedNw = node->stats.nodeJoinedNw;
        nodeReset(node);
        nodeLeftNw++;
        node->stats.nodeLeftNw = nodeLeftNw;
        node->stats.nodeJoinedNw = nodeJoinedNw;
    }
}

void nodeInit(tNode * node, 
                uint64_t uniqueId,
                uint8_t maxTxPacketEntries, 
                tPacketEntry txPacketEntries[], 
                uint8_t maxRxPacketEntries, 
                tPacketEntry rxPacketEntries[],
                tPacketEntry * rxPacketQueue[]) {
    microbusAssert(maxRxPacketEntries > 2, "");
    memset(node, 0, sizeof(tNode));
    node->nodeId = UNALLOCATED_NODE_ID;
    node->uniqueId = uniqueId;

    // Rx
    memset(rxPacketEntries, 0, maxRxPacketEntries * sizeof(tPacketEntry));
    memset(rxPacketQueue, 0, maxRxPacketEntries * sizeof(tPacketEntry *));
    node->rxPacketManager.maxRxPacketEntries = maxRxPacketEntries;
    node->rxPacketManager.rxPacketEntries = rxPacketEntries;
    node->rxPacketManager.rxPacketQueue = rxPacketQueue;
    node->nextRxPacketEntry = findFreeRxPacket(&node->rxPacketManager);
    microbusAssert(node->nextRxPacketEntry, "");

    // Tx
    initTxManager(&node->txManager,
        1,
        &node->txManagerMemory.txSeqNumStart,
        &node->txManagerMemory.txSeqNumEnd,
        &node->txManagerMemory.txSeqNumNext,
        &node->txManagerMemory.txSeqNumPauseCount,
        &node->txManagerMemory.rxSeqNum,
        NULL,
        maxTxPacketEntries,
        txPacketEntries);

    // Fill in an empty packet to start with
    node->nextTxPacket = &node->tmpPacket;
    node->nextTxPacket->protocolVersionAndPacketType = 0;
    node->nextTxPacket->dataSize1 = 0;
    node->nextTxPacket->dataSize2 = 0;
    node->nextTxPacket->node.srcNodeId = 0;
    
    // Could use STM32 random number generator for a better random number
    uint32_t seed = (uniqueId & 0xFFFFFFFF) ^ (uniqueId >> 32);
    customSrand(seed);
    
    node->initialised = true;
}

tPacket * nodeAllocateTxPacketFull(void * node) {
    tNode * rnode = node;
    tPacket * packet = allocateTxPacket(&rnode->txManager, rnode->nodeId);
    if (packet == NULL) {
        rnode->stats.txBufferFull++;
        return NULL;
    }
    return packet;
}

uint8_t * nodeAllocateTxPacket(void * node) {
    tPacket * packet = nodeAllocateTxPacketFull(node);
    if (packet == NULL) {
        return NULL;
    }
    return packet->node.data;
}

void nodeSubmitAllocatedTxPacket(void * node, uint8_t dstNodeId, uint16_t numBytes) {
    tNode * rnode = node;
    if (numBytes > NODE_PACKET_DATA_SIZE) {
        microbusAssert(numBytes <= NODE_PACKET_DATA_SIZE, ""); // "Tx packet exceeds max size"
    }
    tPacket * packet = rnode->txManager.allocatedPacket;
    if (packet == NULL) {
        microbusAssert(0, "");
    }
    submitAllocatedTxPacket(&rnode->txManager, false, NULL, rnode->nodeId, MASTER_NODE_ID, NODE_DATA_PACKET, numBytes);
}

tPacket * nodePeekNextRxDataPacketFull(void * node) {
    tNode * rnode = node;
    tPacketEntry ** entry;
    tRxPacketManager * rpm = &rnode->rxPacketManager;
    if (CIRCULAR_BUFFER_EMPTY(rpm->start, rpm->end, rpm->maxRxPacketEntries)) {
    	return NULL;
    }
    CIRCULAR_BUFFER_PEEK((entry), rpm->rxPacketQueue, rpm->start, rpm->end, rpm->maxRxPacketEntries);
    microbusAssert(entry != NULL, "");
    return &(*entry)->packet;
}

uint8_t * nodePeekNextRxDataPacket(void * node, uint16_t * size, tNodeIndex * srcNodeId) {
    tPacket * packet = nodePeekNextRxDataPacketFull(node);
    if (packet == NULL) {
        return NULL;
    }
    *size = GET_PACKET_DATA_SIZE(packet);
    microbusAssert(*size > 0, "");
    *srcNodeId = MASTER_NODE_ID;
    return packet->master.data;
}

bool nodePopNextDataPacket(void * node) {
    tNode * rnode = node;
    tRxPacketManager * rpm = &rnode->rxPacketManager;
    tPacketEntry ** entry;
    CIRCULAR_BUFFER_PEEK((entry), rpm->rxPacketQueue, rpm->start, rpm->end, rpm->maxRxPacketEntries);
    (*entry)->inUse = false;
    CIRCULAR_BUFFER_POP(rpm->start, rpm->end, rpm->maxRxPacketEntries);
    return true; //TODO
}

