#include "string.h"
#include "stdbool.h"
#include "stdint.h"
#include "stdio.h"
#include "stdlib.h"
#include "microbus.h"
#include "masterTxManager.h"
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

static tTxPacketEntry * findPacketEntry(tMasterTxManager * manager, tNodeIndex nodeId, uint8_t seqNum) {
    for (uint16_t packetIndex=0; packetIndex<MAX_MASTER_TX_PACKETS; packetIndex++) {
        tTxPacketEntry * packetEntry = &manager->txPacket[packetIndex];
        if (packetEntry->valid
            && packetEntry->packet.nodeId == nodeId 
            && packetEntry->packet.txSeqNum == seqNum) {
            return packetEntry;
        }
    }
    myAssert(0, "Failed to find packet");
    return NULL;
}

// NOTE: called by independent thread!
tPacket * addTxPacket(tMasterTxManager * manager, tNodeIndex dstNodeId) {
    tTxPacketEntry * packetEntry = NULL;
    for (uint16_t packetIndex=0; packetIndex<MAX_MASTER_TX_PACKETS; packetIndex++) {
        packetEntry = &manager->txPacket[packetIndex];
        if (!packetEntry->valid) {
            //printf("Storing node:%u, seq:%u in packet:%u\n", dstNodeId, manager->txSeqNumEnd[dstNodeId], packetIndex);
            break;
        }
    }
    if (packetEntry->valid) {
        //myAssert(0, "Tx buffer full");
        return NULL;
    }
    packetEntry->valid = true;
    packetEntry->packet.nodeId = dstNodeId;
    packetEntry->packet.txSeqNum = manager->txSeqNumEnd[dstNodeId];
    // Must be atomic
    manager->txSeqNumEnd[dstNodeId]++;
    // Must be atomic
    manager->numTxIn++;
    return &packetEntry->packet;
}

tPacket * getNextTxPacket(tMasterTxManager * manager, uint8_t nextTxNodeId) {
    if (manager->numTxIn == manager->numTxOut) {
        return NULL;
    }

    // Find next node to transmit to - by start with the last node that transmitted
    // and incrementing to find the next node that needs to transmit
    // NOTE: this will often search all nodes
    myAssert(manager->lastTxNode < MAX_NODES, "Invalid node ID");
    tNodeIndex nodeId = manager->lastTxNode;
    bool found = false;
    do {
        nodeId++;
        if (nodeId == MAX_NODES) {
            nodeId = 0;
        }
        // If packets to send
        if (manager->txSeqNumCurrent[nodeId] != manager->txSeqNumEnd[nodeId]) {
            // If we haven't reached the end of the window
            uint8_t windowEnd = manager->txSeqNumStart[nodeId] + SLIDING_WINDOW_SIZE;
            if (manager->txSeqNumCurrent[nodeId] != windowEnd) {
                found = true;
                break;
            }
        }
    } while (nodeId != manager->lastTxNode);

    // If it doesn't find any other node to transmit then just check it can actually transmit on the lastNode
    if (!found) {
        return NULL;
    }

    // Find the packet with the corresponding txSeqNum
    tTxPacketEntry * packetEntry = findPacketEntry(manager, nodeId, manager->txSeqNumCurrent[nodeId]);
    myAssert(packetEntry != NULL, "Failed to find matching packet!");
    manager->txSeqNumCurrent[nodeId]++;
    manager->lastTxNode = nodeId;
    // Fill in the seq num ()
    packetEntry->packet.ackSeqNum = manager->rxSeqNum[nextTxNodeId];
    return &packetEntry->packet;
}

// Different to typical sliding windows - as we control the protocol we would expect
// an ack immediately after packet sent. So if we don't get it we can move straight
// back to the start (rather than wait for the window to wrap)
// Also we don't have to worry about a backoff as if the other side is down we shouldn't
// rx any acks from them
void rxAckSeqNum(tMasterTxManager * manager, uint8_t nodeId, uint8_t ackSeqNum) {
    myAssert(nodeId < MAX_NODES, "");
    // Check that the sequence number is within the range we are sending
    bool wrapped = manager->txSeqNumEnd[nodeId] < manager->txSeqNumStart[nodeId];
    bool validSeqNum = false;
    if (wrapped) {
        validSeqNum = (ackSeqNum >= manager->txSeqNumStart[nodeId]) || (ackSeqNum < manager->txSeqNumEnd[nodeId]);
    } else {
        validSeqNum = ackSeqNum >= manager->txSeqNumStart[nodeId] && ackSeqNum < manager->txSeqNumEnd[nodeId];
    }
    if (validSeqNum) {
        // Free packets
        for (uint8_t seqNum = manager->txSeqNumStart[nodeId]; seqNum != (uint8_t)(ackSeqNum + 1); seqNum++) {
            tTxPacketEntry * packetEntry = findPacketEntry(manager, nodeId, seqNum);
            if (packetEntry != NULL) {
                //printf("Freed packet for node:%u, seq:%u\n", nodeId, seqNum);
                packetEntry->valid = false;
                manager->numTxOut++;
            }
        }
        // Update the start
        manager->txSeqNumStart[nodeId] = ackSeqNum + 1;
    } else {
        // Reset the window
        manager->txSeqNumCurrent[nodeId] = manager->txSeqNumStart[nodeId];
    }
}
