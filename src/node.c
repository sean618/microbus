#include "string.h"
#include "stdbool.h"
#include "stdint.h"
#include "stdio.h"
#include "stdlib.h"
//#include "scheduler.h"
#include "microbus.h"
#include "node.h"

void halStartnodeTxRxDMA(tNode * node, bool tx, uint8_t * txData, uint8_t * rxData, uint32_t numBytes);
void halStopNodeTxRxDMA(tNode * node);

tPacket * txNewNodeRequest(tNode * node) {
    // TODO: this needs a backoff!
    tPacket * packet = &node->newNodeTxPacket;
    packet->protocolVersionAndPacketType = (MICROBUS_VERSION & 0xF) << 4 | (uint8_t) NEW_NODE_RESPONSE_PACKET;
    packet->srcNodeId = INVALID_NODE_ID;
    //packet->dstNodeId = 0;
    // Set the entire packet to be floating/pulled-up
    memset(packet->data, 0xFF, PACKET_DATA_SIZE);
    // Choose a random offset into the packet (aligned to boundaries)
    uint8_t offset = rand() % (PACKET_DATA_SIZE / NEW_NODE_REQUEST_ENTRY_SIZE);
    uint32_t byteOffset = offset * NEW_NODE_REQUEST_ENTRY_SIZE;
    uint16_t checkSum = (0xFFFF & (node->uniqueId >> 48)) 
                        + (0xFFFF & (node->uniqueId >> 32)) 
                        + (0xFFFF & (node->uniqueId >> 16)) 
                        + (0xFFFF & (node->uniqueId));
    memcpy(&packet->data[byteOffset],   &node->uniqueId, 8);
    memcpy(&packet->data[byteOffset+8], &checkSum,       2);
    MB_PRINTF(node, "Sending new node packet: byteoffset: %u, %lu\n", byteOffset, node->uniqueId);
    return packet;
}

tPacket * nodeProcessTx(tNode * node, tNodeIndex nextTxNodeId) {
    // If we haven't got a node ID then we can send a request when the next slot is unused
    if ((node->nodeId == UNALLOCATED_NODE_ID) && (nextTxNodeId == UNALLOCATED_NODE_ID)) {
        return txNewNodeRequest(node);
    }
    // If we've got a node ID and it's our turn we can transmit
    if ((node->nodeId != UNALLOCATED_NODE_ID) && (nextTxNodeId == node->nodeId)) {
        return nodeTxDataPacket(node);
    }
    return NULL;
}

void rxNewNodePacketResponse(tNode * node, tPacket * packet) {
    uint16_t dataSize = packet->dataSize1 << 8 || packet->dataSize2;
    uint8_t numEntries = dataSize / NEW_NODE_RESPONSE_ENTRY_SIZE;
    for (uint8_t i=0; i<numEntries; i++) {
        uint64_t uniqueId;
        memcpy(&uniqueId, &packet->data[i*NEW_NODE_RESPONSE_ENTRY_SIZE], 8);
        if (uniqueId == node->uniqueId) {
            node->nodeId = packet->data[i*NEW_NODE_RESPONSE_ENTRY_SIZE + 8];
            return;
        }
    }
}

bool isRxPacketCorrupt(tNode * node, tPacket * packet) {
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
    } else if (packetType != DATA_PACKET && packetType != NEW_NODE_RESPONSE_PACKET) {
        myAssert(0, "Invalid packet type");
        return true;
    }
    return false;
}

// Return nextRxPacket
// NOTE: this can't take too long. It must complete before the next packet is transmitted
tPacket * processRxHeaders(tNode * node, tPacket * packet) {
    // By default re-use the rx packet for the next rx as well
    tPacket * nextRxPacket = packet;
    tPacketType packetType = packet->protocolVersionAndPacketType & 0xF;
    
    if (node->nodeId == UNALLOCATED_NODE_ID) {
        if (packetType == NEW_NODE_RESPONSE_PACKET) {
            rxNewNodePacketResponse(node, packet);
        }
        return nextRxPacket;
    }

    // If we have a node ID process data and empty packets
    if (packet->nextTxNodeId == node->nodeId) {
        // The ack from the master is actually for the next node to transmit!
        // Optimisation - this ensures every node gets an ack back from the master before it transmits
        // so the acks always match - even if the master is always transmitting to node 1 and node 2 is
        // always transmitting to master
        rxAckSeqNum(&node->txManager, 0, packet->ackSeqNum);
    }
    
    if (packet->dstNodeId == node->nodeId) {
        if (packetType == DATA_PACKET) {
            if(rxPacketCheckAndUpdateSeqNum(node->txManager, 0, packet->txSeqNum)) {
                if (CIRCULAR_BUFFER_FULL(node->rxPacketsStart, node->rxPacketsEnd, MAX_NODE_RX_PACKETS)) {
                    // If full re-use the previous rx packet so that we can still update our meta-data from the rx headers
                    nextRxPacket = rxPacket;
                    myAssert(0, "Rx packet buffer full!");
                } else {
                    // Leave in the buffer to be processed outside of this driver
                    // Update the tail and use the next free entry
                    node->rxPacketsEnd = INCR_AND_WRAP(node->rxPacketsEnd, 1, MAX_NODE_RX_PACKETS);
                    nextRxPacket = &node->rxPackets[node->rxPacketsEnd];
                }
            }
        }
    }
    return nextRxPacket;
}

void nodeTransferCompleteCb(tNode * node) {
    // TODO: check if we need to stop dma
    // halStopNodeTxRxDMA
    
    // TODO: control the psSignal ourselves to tell the master when we are finished
    // GPIO_set(psPin, LOW)
    
    // From being called to halStartNodeTxRxDMA we need to be as quick as possible!
    
    // Process the rx meta data
    tPacket * rxPacket = &node->rxPackets[node->rxPacketsEnd];
    // By default reuse the rx packet entry
    tPacket * nextRxPacket = rxPacket;
    tPacket * txPacket = NULL;
    
    // We can ignore the packet if it's not for use or telling us we're the next node to Tx
    if (rxPacket->dstNodeId == node->nodeId || rxPacket->nextTxNodeId == node->nodeId) {
        if (!isRxPacketCorrupt(node, rxPacket)) {
            // Process the rx headers first to update the tx's ackSeqNum
            nextRxPacket = processRxHeaders(node, rxPacket);
            txPacket = nodeProcessTx(node, rxPacket->nextTxNodeId);
        }
    }
    
    if (txPacket != NULL) {
        halStartnodeTxRxDMA(node, txPacket->data, (uint8_t *) nextRxPacket, PACKET_SIZE);
    } else {
        halStartnodeRxDMA(node, (uint8_t *) nextRxPacket, PACKET_SIZE);
    }
    
    // Signal that we are ready - when all nodes have released this signal the master can start
    // GPIO_set(psPin, HIGH) // open drain
}

void psLineInterrupt(tNode * node, bool psVal) {
    nodeTransferCompleteCb(node);
}

// ================================ //

void initNode(tNode * node,
        uint8_t numTxNodes,
        uint8_t txSeqNumStart[],
        uint8_t txSeqNumEnd[],
        uint8_t txSeqNumNext[],
        uint8_t rxSeqNum[],
        uint8_t numPacketEntries,
        tTxPacketEntry packetEntries[]) {
    memset(node, 0, sizeof(tNode));
    node->nodeId = UNALLOCATED_NODE_ID;
    initTxManager(&node->txManager, numTxNodes, txSeqNumStart, txSeqNumEnd, txSeqNumNext, rxSeqNum, numPacketEntries, packetEntries);
}

uint8_t * createNodeTxPacketToFillIn(tNode * node, tNodeIndex dstNodeId, uint16_t numBytes) {
    if (numBytes > PACKET_DATA_SIZE) {
        myAssert(0, "Tx packet exceeds max size");
        return NULL;
    }
    if (node->nodeId == UNALLOCATED_NODE_ID || node->nodeId > MAX_NODES) {
        myAssert(0, "Node not initialised yet");
        return NULL;
    }
    tPacket * packet = allocateTxPacket(&node->txManager, node->nodeId, dstNodeId);
    if (packet != NULL) {
        packet->dataSize1 = packetSize >> 8;
        packet->dataSize2 = packetSize & 0xFF;
    }
    return packet;
}