#include "string.h"
#include "stdbool.h"
#include "stdint.h"
#include "stdio.h"
#include "stdlib.h"
//#include "scheduler.h"
#include "microbus.h"
#include "master.h"
#include "txManager.h"
#include "newNode.h"


static bool isRxPacketCorrupt(tPacket * packet) {
    bool crcValid = true;
    uint8_t protocolVersion = (packet->protocolVersionAndPacketType >> 4) & 0xF;
    tPacketType packetType = packet->protocolVersionAndPacketType & 0xF;
    
    if (!crcValid) {
        myAssert(0, "Invalid CRC");
        // TODO: increment counter
        return true;
    } else if (protocolVersion != MICROBUS_VERSION) {
        myAssert(0, "Invalid version");
        // TODO: increment counter
        return true;
    } else if (packetType != DATA_PACKET && packetType != NEW_NODE_REQUEST_PACKET) {
        myAssert(0, "Invalid packet type");
        return true;
    }
    return false;
}

// Return pointer to the buffer entry for the next rx packet
// NOTE: this can't take too long. It must complete before the next packet is transmitted
static tPacket * masterProcessRxHeaders(tMasterNode * master, tPacket * packet) {
    // By default re-use the rx packet for the next rx as well
    tPacket * nextRxPacket = packet;
    tPacketType packetType = packet->protocolVersionAndPacketType & 0xF;
    
    rxAckSeqNum(&master->txManager, packet->srcNodeId, packet->ackSeqNum);
    
    if (packetType == DATA_PACKET) {
        if(rxPacketCheckAndUpdateSeqNum(&master->txManager, packet->srcNodeId, packet->txSeqNum)) {
            if (CIRCULAR_BUFFER_FULL(master->rxPacketsStart, master->rxPacketsEnd, master->maxRxPackets)) {
                // If full re-use the previous rx packet so that we can still update our meta-data from the rx headers
                nextRxPacket = packet;
                myAssert(0, "Rx packet buffer full!");
            } else {
                // Leave in the buffer to be processed outside of this driver
                // Update the tail and use the next free entry
                master->rxPacketsEnd = INCR_AND_WRAP(master->rxPacketsEnd, 1, master->maxRxPackets);
                nextRxPacket = &master->rxPackets[master->rxPacketsEnd];
            }
        }
    } else if (packetType == NEW_NODE_REQUEST_PACKET) {
        rxNewNodePacketRequest(master, packet);
    }
    return nextRxPacket;
}

static tPacket * masterProcessTx(tMasterNode * master) {
    //if (rxPacket->nextTxNodeId == master->nodeId) {
        // If we've got a node ID and it's our turn we can transmit
        return getNextTxPacket(&node->txManager);
    //}
    //return NULL;
}

// ========================================= //

void masterProcessRxAndGetTx(tMasterNode * master, tPacket ** txPacket, tPacket ** nextRxPacket) {
    tPacket * rxPacket = &master->rxPackets[master->rxPacketsEnd];
    // By default reuse the rx packet entry
    *nextRxPacket = rxPacket;
    *txPacket = NULL;
    
    // We can ignore the packet if it's not for use or telling us we're the next node to Tx
    if (rxPacket->dstNodeId == master->nodeId) {
        if (!isRxPacketCorrupt(rxPacket)) {
            *nextRxPacket = masterProcessRxHeaders(master, rxPacket);
        }
    }
    *txPacket = masterProcessTx(master, rxPacket->nextTxNodeId);
}


// ========================================= //
// Called by interupt

void nodeProcessRxAndGetTx(tNode * node, tPacket ** txPacket, tPacket ** nextRxPacket) {
    tPacket * rxPacket = &node->rxPackets[node->rxPacketsEnd];
    // By default reuse the rx packet entry
    *nextRxPacket = rxPacket;
    *txPacket = NULL;
    
    // We can ignore the packet if it's not for use or telling us we're the next node to Tx
    if (rxPacket->dstNodeId == node->nodeId || rxPacket->nextTxNodeId == node->nodeId) {
        if (!isNodeRxPacketCorrupt(rxPacket)) {
            *nextRxPacket = nodeProcessRxHeaders(node, rxPacket);
        }
    }
    *txPacket = nodeProcessTx(node, rxPacket);
    (*nextRxPacket)->protocolVersionAndPacketType = 0;
}

// ========================================= //
// Called by main thread

void masterInit(tMasterNode * master, uint8_t maxTxPacketEntries, tTxPacketEntry txPacketEntries[], uint8_t maxRxPackets, tPacket rxPackets[]) {
    memset(master, 0, sizeof(tMasterNode));
    initTxManager(&master->txManager, MAX_TX_NODES, master->txSeqNumStart, master->txSeqNumEnd, master->txSeqNumNext, master->rxSeqNum, maxTxPacketEntries, txPacketEntries);
    //master->nodeId = 0; // TODO: MASTER NODE ID
    master->maxRxPackets = maxRxPackets;
    master->rxPackets = rxPackets;
}

uint8_t * masterAllocateTxPacket(tMasterNode * master, tNodeIndex dstNodeId, uint16_t numBytes) {
    if (numBytes > PACKET_DATA_SIZE) {
        myAssert(0, "Tx packet exceeds max size");
        return NULL;
    }
    tPacket * packet = allocateTxPacket(&master->txManager, master->nodeId, dstNodeId);
    if (packet != NULL) {
        packet->dataSize1 = numBytes >> 8;
        packet->dataSize2 = numBytes & 0xFF;
    }
    return packet;
}

tPacket * masterPeekNextRxDataPacket(tMasterNode * master) {
    tPacket * entry;
    CIRCULAR_BUFFER_PEEK(entry, master->rxPackets, master->rxPacketsStart, master->rxPacketsEnd, master->maxRxPackets);
    return entry;
}

void masterPopNextDataPacket(tMasterNode * master) {
    CIRCULAR_BUFFER_POP(master->rxPacketsStart, master->rxPacketsEnd, master->maxRxPackets);
}

