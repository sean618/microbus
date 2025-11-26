// Copyright (c) 2025 Sean Bremner
// Licensed under the MIT License. See LICENSE file for details.

#include "string.h"
#include "stdbool.h"
#include "stdint.h"
#include "stdio.h"
#include "stdlib.h"

#include "microbus.h"
#include "txManager.h"

// =============================================================== //
//                        Tx Manager
// 
// This manages which packet to transmit next. The main feature
// is that it ensures no packets are dropped and they are received in order. 
//
// This is done using a sliding window protocol (like TCP), where
// the tx sequentially transmits packets until it reaches the end
// of it's window. At the same time the receiver should be sending 
// acknowledgements back moving the start of the window. 
// If after a certain amount of time the receiver has not acknowledged
// a packet then the tx will start from the beginning of the window 
// (retransmitting any lost packets).
//
// Not very efficient if there is a high rate of dropped packets but it's
// very efficient if there is a low rate of dropped packets. Plus it
// ensures correct ordering at the rx.
//
// =============================================================== //

//#define SLIDING_WINDOW_TIMEOUT_RX_PACKETS 3 // How many rx packets without an update to the startSeqNum before we go transmit from the start of the window


#define INCR_SEQUENCE_NUM(seqNum) (seqNum)++; if ((seqNum) >= MAX_SEQUENCE_NUM) {(seqNum) = 0;}

// =============================================================== //
// Packet store
// It's a bit inefficient that we are constantly search to find new or matching packets
// However given the packets are reasonably large we are unlikely to be able to store
// many of them in a microcontroller so there shouldn't be that many to search

static tPacketEntry * findPacketEntry(tPacketStore * store, tNodeIndex dstNodeId, uint8_t seqNum, bool isMaster) {
    for (uint16_t packetIndex=0; packetIndex<store->maxEntries; packetIndex++) {
        tPacketEntry * packetEntry = &store->entries[packetIndex];
        bool dstNodeIdMatches = isMaster ? (packetEntry->packet.master.dstNodeId == dstNodeId) : true;
        if (packetEntry->inUse
            && dstNodeIdMatches
            && packetEntry->packet.txSeqNum == seqNum) {
            return packetEntry;
        }
    }
    microbusAssert(0, ""); // "Failed to find packet"
    return NULL;
}

static tPacket * allocatePacketEntry(tPacketStore * store) {
    tPacket * result = NULL;
    for (uint16_t packetIndex=0; packetIndex<store->maxEntries; packetIndex++) {
        if (!store->entries[packetIndex].inUse) {
            store->entries[packetIndex].inUse = true;
            store->numStored++;
            result = &store->entries[packetIndex].packet;
            break;
        }
    }
    return result;
}

static bool freePacket(tPacketStore * store, tNodeIndex dstNodeId, uint8_t seqNum, bool isMaster) {
    tPacketEntry * entry = findPacketEntry(store, dstNodeId, seqNum, isMaster);
    if (entry != NULL) {
        store->numFreed++;
        entry->inUse = false;
        return true;
    }
    return false;
}

uint8_t getNumAllBufferedTxPackets(tTxManager * manager) {
    uint8_t count = 0;
    for (uint16_t packetIndex=0; packetIndex<manager->packetStore.maxEntries; packetIndex++) {
        if (manager->packetStore.entries[packetIndex].inUse) {
            count++;
        }
    }
    return count;
}

// =============================================================== //
// Windowing/retransmit logic - Generic to both Master and Node

// NOTE: called by independent thread
tPacket * allocateTxPacket(tTxManager * manager, uint8_t nodeId) {
    tPacket * packet = allocatePacketEntry(&manager->packetStore);
    if (packet == NULL) {
        // MB_TX_MANAGER_PRINTF("%s:%u Tx buffer full\n", nodeId == MASTER_NODE_ID ? "Master" : "Node", nodeId);
        return NULL;
    }
    if (manager->allocatedPacket != NULL) {
        microbusAssert(0, ""); // "Allocated packet must be submitted before the next allocation"
        return NULL;
    }
    if (manager->activeTxNodes) {
        if (manager->activeTxNodes->numNodes >= MAX_NODES) {
            // MB_TX_MANAGER_PRINTF("%s:%u Tx activeTxQueue is full\n", nodeId == MASTER_NODE_ID ? "Master" : "Node", nodeId);
            return NULL;
        }
    }
    manager->allocatedPacket = packet;
    return packet;
}

// NOTE: called by independent thread (lower priority thread)
void submitAllocatedTxPacket(tTxManager * manager, bool isMaster, uint8_t * masterDstNodeTTL, tNodeIndex srcNodeId, tNodeIndex dstNodeId, tPacketType packetType, uint16_t dataSize) {
    if (isMaster) {
        if (*masterDstNodeTTL <= 0) {
            manager->allocatedPacket = NULL;
            return;
        }
    }
    microbusAssert(srcNodeId < MAX_NODES && dstNodeId < MAX_NODES, "");
    tPacket * packet = manager->allocatedPacket;
    SET_PROTOCOL_VERSION_AND_PACKET_TYPE(packet, packetType);
    SET_PACKET_DATA_SIZE(packet, dataSize);
    if (isMaster) {
        packet->master.dstNodeId = dstNodeId;
    } else {
    	microbusAssert(dstNodeId == 0, "");
        microbusAssert(packetType != MASTER_DATA_PACKET, "");
        packet->node.srcNodeId = srcNodeId;
    }
    packet->txSeqNum = manager->txSeqNumEnd[dstNodeId];

    // Update to txSeqNumEnd must be atomic - for different threads
    INCR_SEQUENCE_NUM(manager->txSeqNumEnd[dstNodeId]);

    if (isMaster) {
        // If buffer was empty then add this node to the record of active nodes
        if (packet->txSeqNum == manager->txSeqNumStart[dstNodeId]) {
            if (nodeQueueAdd(manager->activeTxNodes, dstNodeId) == false) {
                microbusAssert(0, ""); // Should be caught by allocate
            }
            MB_TX_MANAGER_PRINTF("%s %u->%u adding to activeTxQueue (num:%u), seqNum:%u\n", isMaster ? "Master" : "Node", srcNodeId, dstNodeId, manager->activeTxNodes->numNodes, packet->txSeqNum);
        }
    }

    // Keep a record of how full the buffer gets
    uint8_t txBufferLevel = manager->txSeqNumEnd[dstNodeId] - manager->txSeqNumStart[dstNodeId];
    manager->txBufferLevel = txBufferLevel;
    manager->maxTxBufferLevel = MAX(manager->maxTxBufferLevel, txBufferLevel);

    manager->allocatedPacket = NULL;

    // This function could be interrupted at any point by a thread that could remove the node
    // If that has happened check now and remove it too
    if (isMaster) {
        if (*masterDstNodeTTL == 0) {
            uint8_t unusedNumTxPacketsFreed;
            masterTxManagerRemoveNode(manager, dstNodeId, &unusedNumTxPacketsFreed);
        }
    }

    // MB_TX_MANAGER_PRINTF("%s %u, dst:%u, submitting packet, txSeqNum:%u, dataSize:%u\n", isMaster ? "Master" : "Node", srcNodeId, dstNodeId, packet->txSeqNum, dataSize);
}

inline static tPacketEntry * getNextTxPacketForNode(tTxManager * manager, bool isMaster, tNodeIndex dstNodeId) {
    uint8_t start        = manager->txSeqNumStart[dstNodeId];
    uint8_t end          = manager->txSeqNumEnd[dstNodeId];
    uint8_t * next       = &manager->txSeqNumNext[dstNodeId];
    uint8_t * pauseCount = &manager->txSeqNumPauseCount[dstNodeId];

    if (*pauseCount > 0) {
        return NULL;
    }

    // Should only be called if we know it's active
    if (start == end) {
        return NULL;
    }

    if (end >= start) {
        microbusAssert(*next >= start && *next <= end, "");
    } else {
        microbusAssert(*next >= start || *next <= end, "");
    }
    
    uint8_t windowEnd = start + SLIDING_WINDOW_SIZE;
    if ((*next == end) || (*next == windowEnd)) {
        // if (*pauseCount == SLIDING_WINDOW_PAUSE) {
        //     MB_TX_MANAGER_PRINTF("%s -> %u: Reached end of window, restarting window\n", isMaster ? "Master" : "Node", dstNodeId);
        //     *next = start;
        //     *pauseCount = 0;
        // } else {
            MB_TX_MANAGER_PRINTF("%s -> %u: Reached end of window, pausing\n", isMaster ? "Master" : "Node", dstNodeId);
            (*pauseCount) = 2; // wait for 2 acks before restarting
            return NULL;
        // }
    }
    
    tPacketEntry * packetEntry = findPacketEntry(&manager->packetStore, dstNodeId, *next, isMaster);
    if (packetEntry == NULL) {
        microbusAssert(0, ""); // "Failed to find matching packet!"
        return NULL;
    }
    INCR_SEQUENCE_NUM((*next));
    return packetEntry;
}

tPacket * nodeGetNextTxDataPacket(tTxManager * manager) {
    if (manager->txSeqNumStart[MASTER_NODE_ID] == manager->txSeqNumEnd[MASTER_NODE_ID]) {
        // Empty
        return NULL;
    }

    tPacketEntry * packetEntry = getNextTxPacketForNode(manager, false, MASTER_NODE_ID);
    // Fill in any acks we need to send
    if (packetEntry) {
        packetEntry->packet.node.ackSeqNum = manager->rxSeqNum[MASTER_NODE_ID];
    }

    return packetEntry ? &packetEntry->packet : NULL;
}

tPacket * masterGetNextTxDataPacket(tTxManager * manager, uint8_t numTxNodesScheduled, uint8_t nextTxNodeId[MAX_TX_NODES_SCHEDULED], uint8_t burstSize) {
    tNodeQueue * activeTxNodes = manager->activeTxNodes;
    // Find next node to transmit to - by start with the last node that transmitted
    // and incrementing to find the next node that needs to transmit
    tPacketEntry * packetEntry = NULL;
    if (activeTxNodes->numNodes == 0) {
        MB_TX_MANAGER_PRINTF("Master: no active tx nodes\n");
        return NULL;
    }

    uint32_t lastQueueIndex = manager->lastTxQueueIndex;
    if (lastQueueIndex >= activeTxNodes->numNodes) {
    	lastQueueIndex = activeTxNodes->numNodes - 1;
    }

    // On single channel, to avoid half the packets having to be acks we try and 
    // send bursts of packets to a single dst
    if (burstSize > 1) {
        if (manager->lastTxQueueCount < burstSize) {
            tNodeIndex dstNodeId = activeTxNodes->nodeIds[lastQueueIndex];
            packetEntry = getNextTxPacketForNode(manager, true, dstNodeId);
            if (packetEntry) {
                manager->lastTxQueueCount++;
                return &packetEntry->packet;
            }
        }
    }

    MB_TX_MANAGER_PRINTF("Master, getNextTxPacket checking nodes:");
    uint32_t i = lastQueueIndex;
    do {
        i++;
        if (i >= activeTxNodes->numNodes) {
            i = 0;
        }
        tNodeIndex dstNodeId = activeTxNodes->nodeIds[i];
        if (MICROBUS_LOG_TX_MANGER > 0) { MB_PRINTF_WITHOUT_NEW_LINE("%u, ", dstNodeId) }
        packetEntry = getNextTxPacketForNode(manager, true, dstNodeId);
    } while (i != lastQueueIndex && packetEntry == NULL);
    if (MICROBUS_LOG_TX_MANGER > 0) { MB_PRINTF_WITHOUT_NEW_LINE("\n") }

    manager->lastTxQueueIndex = i;

    if (packetEntry) {
        manager->lastTxQueueCount = 1;
        return &packetEntry->packet;
    }
    return NULL;
}

// Return num tx packet freed
uint8_t rxAckSeqNum(tTxManager * manager, tNodeIndex srcNodeId, uint8_t ackSeqNum, bool isMaster, uint64_t * statsNumTxWindowRestarts) {
    // If invalid - just means we haven't started sending data yet
    if (ackSeqNum == INVALID_SEQUENCE_NUM) {
        return 0;
    }

    uint8_t packetsFreed = 0;
    uint8_t * start      = &manager->txSeqNumStart[srcNodeId];
    uint8_t * end        = &manager->txSeqNumEnd[srcNodeId];
    uint8_t * next       = &manager->txSeqNumNext[srcNodeId];
    uint8_t * pauseCount = &manager->txSeqNumPauseCount[srcNodeId];

    // Check that the sequence number is within the range we are sending
    bool validSeqNum = false;
    bool wrapped = *end < *start;
    if (wrapped) {
        validSeqNum = (ackSeqNum >= *start) || (ackSeqNum < *end);
    } else {
        validSeqNum = ackSeqNum >= *start && ackSeqNum < *end;
    }
    if (validSeqNum) {
        // MB_TX_MANAGER_PRINTF("%s -> %u: good ack seqnum, start:%u, end:%u, got:%u\n", isMaster ? "Master" : "Node", srcNodeId, *start, *end, ackSeqNum);

        uint8_t newStart = ackSeqNum;
        INCR_SEQUENCE_NUM(newStart);

        for (uint8_t seqNum = *start; seqNum != newStart; ) {
            // Free packets
            bool valid = freePacket(&manager->packetStore, srcNodeId, seqNum, isMaster);
            microbusAssert(valid, "");
            packetsFreed++;
            // Update the next if it happened to have restarted before the ack
            if (seqNum == *next) {
                *next = newStart;
            }
            INCR_SEQUENCE_NUM(seqNum)
        }
        // Update the start
        *start = newStart;
        // Reset the pause count
        *pauseCount = 0;

        // If the buffer is now empty remove this node from the record of active nodes
        if ((*start == *end) && isMaster) {
            nodeQueueRemove(manager->activeTxNodes, srcNodeId);
            microbusAssert(manager->txSeqNumStart[srcNodeId] == manager->txSeqNumEnd[srcNodeId], "");
            MB_TX_MANAGER_PRINTF("%s: srcNode:%u remove from activeTxQueue (num left:%u)\n", isMaster ? "Master" : "Node", srcNodeId, manager->activeTxNodes->numNodes);
        }
    } else {
        MB_TX_MANAGER_PRINTF("%s - src:%u, invalid ack seqnum, start:%u, end:%u, got:%u\n", isMaster ? "Master" : "Node", srcNodeId, *start, *end, ackSeqNum);
        // We pause for a few acks - if no valid acks in that time then restart
        if (*pauseCount > 0) {
            (*pauseCount)--;
            if (*pauseCount == 0) {
                // This shouldn't really happen unless packets have been dropped, right? - TODO
                MB_TX_MANAGER_PRINTF("%s -> %u: Restarting window\n", isMaster ? "Master" : "Node", srcNodeId);
                (*statsNumTxWindowRestarts)++;
                *next = *start;
            }
        }
    }
    return packetsFreed;
}

// Check that the received packet has the next expected sequence number
bool rxPacketCheckAndUpdateSeqNum(tTxManager * manager, tNodeIndex srcNodeId, uint8_t packetTxSeqNum, bool isMaster) {
    uint8_t expectedSeqNum = manager->rxSeqNum[srcNodeId];
    if (expectedSeqNum == NULL_SEQUENCE_NUM) {
        expectedSeqNum = 0;
    } else {
        INCR_SEQUENCE_NUM(expectedSeqNum)
    }

    if (packetTxSeqNum == expectedSeqNum) {
        // MB_TX_MANAGER_PRINTF("%s -> %u: good seqnum, exp:%u, got:%u\n", isMaster ? "Master" : "Node", srcNodeId, expectedSeqNum, packetTxSeqNum);
        manager->rxSeqNum[srcNodeId] = packetTxSeqNum;
        return true;
    }
    MB_TX_MANAGER_PRINTF("%s - src:%u: bad rx seqnum, exp:%u, got:%u\n", isMaster ? "Master" : "Node", srcNodeId, expectedSeqNum, packetTxSeqNum);
    return false;
}

uint8_t getNumInTxBuffer(tTxManager * manager, uint8_t dstNodeId) {
    return (manager->txSeqNumEnd[dstNodeId] - manager->txSeqNumStart[dstNodeId]);
}

void masterTxClearBuffers(tTxManager * manager) {
    for (uint16_t packetIndex=0; packetIndex < manager->packetStore.maxEntries; packetIndex++) {
        tPacketEntry * packetEntry = &manager->packetStore.entries[packetIndex];
        if (packetEntry->inUse) {
            packetEntry->inUse = false;
            manager->packetStore.numFreed++;
        }
    }
}

void masterTxManagerRemoveNode(tTxManager * manager, tNodeIndex nodeId, uint8_t * numTxPacketsFreed) {
    nodeQueueRemoveIfExists(manager->activeTxNodes, nodeId);
    for (uint16_t packetIndex=0; packetIndex < manager->packetStore.maxEntries; packetIndex++) {
        tPacketEntry * packetEntry = &manager->packetStore.entries[packetIndex];
        if (packetEntry->packet.master.dstNodeId == nodeId && packetEntry->inUse) {
            packetEntry->inUse = false;
            manager->packetStore.numFreed++;
            (*numTxPacketsFreed)++;
        }
    }
    manager->txSeqNumStart[nodeId] = 0;
    manager->txSeqNumEnd[nodeId] = 0;
    manager->txSeqNumNext[nodeId] = 0;
    manager->txSeqNumPauseCount[nodeId] = 0;
    manager->rxSeqNum[nodeId] = NULL_SEQUENCE_NUM;
}

void initTxManager(
        tTxManager * manager,
        uint8_t maxTxNodes,
        uint8_t txSeqNumStart[],
        uint8_t txSeqNumEnd[],
        uint8_t txSeqNumNext[],
        uint8_t txSeqNumPauseCount[],
        uint8_t rxSeqNum[],
        tNodeQueue * activeTxNodes,
        uint8_t maxPacketEntries,
        tPacketEntry packetEntries[]
    ) {
    // Zero all passed in memory
    for (uint8_t nodeId=0; nodeId<maxTxNodes; nodeId++) {
        txSeqNumStart[nodeId] = 0;
        txSeqNumEnd[nodeId] = 0;
        txSeqNumNext[nodeId] = 0;
        txSeqNumPauseCount[nodeId] = 0;
        rxSeqNum[nodeId] = NULL_SEQUENCE_NUM;
    }
    memset(manager, 0, sizeof(tTxManager));

    manager->maxTxNodes = maxTxNodes;
    manager->txSeqNumStart = txSeqNumStart;
    manager->txSeqNumEnd = txSeqNumEnd;
    manager->txSeqNumNext = txSeqNumNext;
    manager->txSeqNumPauseCount = txSeqNumPauseCount;
    manager->rxSeqNum = rxSeqNum;
    manager->packetStore.maxEntries = maxPacketEntries;
    manager->packetStore.entries = packetEntries;
    manager->activeTxNodes = activeTxNodes;

    for (uint8_t i=0; i<maxPacketEntries; i++) {
        manager->packetStore.entries[i].inUse = false;
    }
}
