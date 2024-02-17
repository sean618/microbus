#include "string.h"
#include "stdbool.h"
#include "stdint.h"
#include "stdio.h"
#include "stdlib.h"
#include "microbus.h"
#include "txManager.h"
#include "usefulLib.h"

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

#define SLIDING_WINDOW_SIZE 3 // The size of the sliding window in tx packets
//#define SLIDING_WINDOW_TIMEOUT_RX_PACKETS 3 // How many rx packets without an update to the startSeqNum before we go transmit from the start of the window

// =============================================================== //
// Packet store
// It's a bit inefficient that we are constantly search to find new or matching packets
// However given the packets are reasonably large we are unlikely to be able to store
// many of them in a microcontroller so there shouldn't be that many to search

static tTxPacketEntry * findPacketEntry(tPacketStore * store, tNodeIndex dstNodeId, uint8_t seqNum) {
    for (uint16_t packetIndex=0; packetIndex<store->numEntries; packetIndex++) {
        tTxPacketEntry * packetEntry = &store->entries[packetIndex];
        if (packetEntry->valid
            && packetEntry->packet.dstNodeId == dstNodeId 
            && packetEntry->packet.txSeqNum == seqNum) {
            return packetEntry;
        }
    }
    myAssert(0, "Failed to find packet");
    return NULL;
}

static tPacket * allocatePacketEntry(tPacketStore * store) {
    for (uint16_t packetIndex=0; packetIndex<store->numEntries; packetIndex++) {
        if (!store->entries[packetIndex].valid) {
            store->entries[packetIndex].valid = true;
            store->numStored++;
            return &store->entries[packetIndex].packet;
        }
    }
    return NULL;
}

static void freePacket(tPacketStore * store, tNodeIndex dstNodeId, uint8_t seqNum) {
    tTxPacketEntry * entry = findPacketEntry(store, dstNodeId, seqNum);
    if (entry != NULL) {
        store->numFreed++;
        entry->valid = false;
    }
}

// =============================================================== //
// Windowing/retransmit logic - Generic to both Master and Node

void initTxManager(
    tTxManager * manager,
    uint8_t numTxNodes,
    uint8_t txSeqNumStart[],
    uint8_t txSeqNumEnd[],
    uint8_t txSeqNumNext[],
    uint8_t rxSeqNum[],
    uint8_t numPacketEntries,
    tTxPacketEntry packetEntries[]) {
    
    memset(manager, 0, sizeof(tTxManager));
    for (uint8_t i=0; i<numTxNodes; i++) {
        txSeqNumStart[i] = 0;
        txSeqNumEnd[i] = 0;
        txSeqNumNext[i] = 0;
        rxSeqNum[i] = 0;
    }
    for (uint8_t i=0; i<numPacketEntries; i++) {
        packetEntries[i].valid = false;
    }
    
    manager->numTxNodes = numTxNodes;
    manager->txSeqNumStart = txSeqNumStart;
    manager->txSeqNumEnd = txSeqNumEnd;
    manager->txSeqNumNext = txSeqNumNext;
    manager->rxSeqNum = rxSeqNum;
    manager->packetStore.numEntries = numPacketEntries;
    manager->packetStore.entries = packetEntries;
}

// NOTE: called by independent thread!
tPacket * allocateTxPacket(tTxManager * manager, tNodeIndex srcNodeId, tNodeIndex dstNodeId) {
    tPacket * packet = allocatePacketEntry(&manager->packetStore);
    if (packet == NULL) {
        //myAssert(0, "Tx buffer full");
        return NULL;
    }
    packet->srcNodeId = srcNodeId;
    packet->dstNodeId = dstNodeId;
    packet->txSeqNum = manager->txSeqNumEnd[dstNodeId];
    // Must be atomic
    manager->txSeqNumEnd[dstNodeId]++;
    return packet;
}

// TODO: for the master rxSeqNum is for next tx slot node
static tPacket * getNextTxPacketForNode(tPacketStore * store, tNodeIndex nodeId, uint8_t * txSeqNumStart, uint8_t * txSeqNumEnd, uint8_t * txSeqNumNext, uint8_t rxSeqNum) {
    if (*txSeqNumNext == *txSeqNumEnd) {
        return NULL;
    }
    uint8_t windowEnd = *txSeqNumStart + SLIDING_WINDOW_SIZE;
    if (*txSeqNumNext == windowEnd) {
        return NULL;
    }
    tTxPacketEntry * packetEntry = findPacketEntry(store, nodeId, *txSeqNumNext);
    if (packetEntry == NULL) {
        myAssert(0, "Failed to find matching packet!");
        return NULL;
    }
    (*txSeqNumNext)++;
    packetEntry->packet.ackSeqNum = rxSeqNum;
    return &packetEntry->packet;
}

tPacket * getNextTxPacket(tTxManager * manager) {
    // Find next node to transmit to - by start with the last node that transmitted
    // and incrementing to find the next node that needs to transmit
    // This is all for the master - for a node this will all just be the same 
    // as calling getNextTxPacketForNode
    
    myAssert(manager->lastTxNode < manager->numTxNodes, "Invalid node ID");
    tNodeIndex dstNodeId = manager->lastTxNode;
    do {
        dstNodeId++;
        if (dstNodeId >= manager->numTxNodes) {
            dstNodeId = 0;
        }
        // If packets to send
        tPacket * packet = getNextTxPacketForNode(
                                &manager->packetStore,
                                dstNodeId,
                                &manager->txSeqNumStart[dstNodeId],
                                &manager->txSeqNumEnd[dstNodeId],
                                &manager->txSeqNumNext[dstNodeId],
                                manager->rxSeqNum[dstNodeId]);
        if (packet != NULL) {
            return packet;
        }
    } while (dstNodeId != manager->lastTxNode);
    return NULL;
}

// Different to typical sliding windows - as we control the protocol we would expect
// an ack immediately after packet sent. So if we don't get it we can move straight
// back to the start (rather than wait for the window to wrap)
// Also we don't have to worry about a backoff as if the other side is down we shouldn't
// rx any acks from them
static void rxAckSeqNumForNode(tPacketStore * store, tNodeIndex nodeId, uint8_t * txSeqNumStart, uint8_t * txSeqNumEnd, uint8_t * txSeqNumNext, uint8_t ackSeqNum) {
    // Check that the sequence number is within the range we are sending
    bool wrapped = *txSeqNumEnd < *txSeqNumStart;
    bool validSeqNum = false;
    if (wrapped) {
        validSeqNum = (ackSeqNum >= *txSeqNumStart) || (ackSeqNum < *txSeqNumEnd);
    } else {
        validSeqNum = ackSeqNum >= *txSeqNumStart && ackSeqNum < *txSeqNumEnd;
    }
    if (validSeqNum) {
        uint8_t newStart = ackSeqNum + 1;
        for (uint8_t seqNum = *txSeqNumStart; seqNum != newStart; seqNum++) {
            // Free packets
            freePacket(store, nodeId, seqNum);
            // Update the next
            if (seqNum == *txSeqNumNext) {
                *txSeqNumNext = newStart;
            }
        }
        // Update the start
        *txSeqNumStart = newStart;
    } else {
        // Reset the window
        *txSeqNumNext = *txSeqNumStart;
    }
}

void rxAckSeqNum(tTxManager * manager, tNodeIndex nodeId, uint8_t ackSeqNum) {
    rxAckSeqNumForNode(
        &manager->packetStore,
        nodeId,
        &manager->txSeqNumStart[nodeId],
        &manager->txSeqNumEnd[nodeId],
        &manager->txSeqNumNext[nodeId],
        ackSeqNum
    );
}

bool rxPacketCheckAndUpdateSeqNum(tTxManager * manager, tNodeIndex srcNodeId, uint8_t packetTxSeqNum) {
    if (packetTxSeqNum == (manager->rxSeqNum[srcNodeId] + 1)) {
        manager->rxSeqNum[srcNodeId]++;
        return true;
    }
    return false;
}
