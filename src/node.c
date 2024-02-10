#include "string.h"
#include "stdbool.h"
#include "stdint.h"
#include "stdio.h"
#include "stdlib.h"
//#include "scheduler.h"
#include "microbus_core.h"
#include "microbus.h"

void halStartnodeTxRxDMA(tNode * node, bool tx, uint8_t * txData, uint8_t * rxData, uint32_t numBytes);
void halStopNodeTxRxDMA(tNode * node);

void nodeRxDataPacket(tNode * node, tPacket * packet) {
    uint16_t dataSize = packet->dataSize1 << 8 || packet->dataSize2;
    if (dataSize > 0) {
        //MB_PRINTF(node, "Node rx:%d\n", node->rxPacket[node->rxPacketsReceived].data[0]);
        myAssert(node->rxPacketsReceived < MAX_RX_PACKETS, "Rx packet buffer overflow");
        node->rxPacketsReceived++;
    }
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

void nodeProcessRx(tNode * node, tPacket * packet) {
    tPacket * packet = &node->rxPacket[node->rxPacketsReceived];
    uint8_t protocolVersion = (packet->protocolVersionAndPacketType >> 4) & 0xF;
    tPacketType packetType = packet->protocolVersionAndPacketType & 0xF;

    if (protocolVersion != MICROBUS_VERSION) {
        myAssert(0, "Invalid version");
        return;
    }
    if (packetType == EMPTY_PACKET) {
        return;
    }
    if (packetType == NEW_NODE_RESPONSE_PACKET) {
        if (node->nodeId == INVALID_NODE_ID) {
            rxNewNodePacketResponse(node, packet);
        }
    } else if (packetType == DATA_PACKET) {
        if (packet->nodeId == node->nodeId) {
            nodeRxDataPacket(node, packet);
        }
    } else {
        myAssert(0, "Master received invalid packet type during DATA_MODE");
    }
}

tPacket * txNewNodeRequest(tNode * node) {
    // TODO: this needs a backoff!
    tPacket * packet = &node->newNodeTxPacket;
    packet->protocolVersionAndPacketType = (MICROBUS_VERSION & 0xF) << 4 | (uint8_t) NEW_NODE_RESPONSE_PACKET;
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
    if ((node->nodeId == INVALID_NODE_ID) && (nextTxNodeId == UNUSED_NODE_ID)) {
        return txNewNodeRequest(node);
    }
    // If we've got a node ID and it's our turn we can transmit
    if ((node->nodeId != INVALID_NODE_ID) && (nextTxNodeId == node->nodeId)) {
        return nodeTxDataPacket(node);
    }
    return NULL;
}

void nodeTransferCompleteCb(tNode * node) {
    // TODO: check if we need to stop dma
    // halStopNodeTxRxDMA
    
    // From being called to halStartNodeTxRxDMA we need to be as quick as possible!
    // halStartNodeTxRxDMA must be started before the master starts its TxRx

    tPacket * rxPacket = &node->rxPacket[node->rxPacketsReceived];
    tPacket * txPacket = nodeProcessTx(node, rxPacket->nextTxNodeId);

    // If there is nothing to send we still need to provide the dma with something
    // And that must be all 1's as the bus is pulled high
    node->tx = (txPacket != NULL);
    // #ifdef LOGGING
    //     MB_PRINTF(node, "node %u: mode:%u, slot:%u, tx:%u, numSlots:%u\n", node->nodeId, node->mode, node->currentSlot, node->tx, node->scheduler.numSlots);
    // #endif    

    myAssert(node->rxPacketsReceived < MAX_RX_PACKETS, "Received more rx packets than buffer size");
    uint8_t * rxData = node->rxPacket[node->rxPacketsReceived].data;
    if (node->tx) {
        halStartnodeTxRxDMA(node, txPacket->data, rxData, PACKET_SIZE);
    } else {
        halStartnodeRxDMA(node, rxData, PACKET_SIZE);
    }
    nodeProcessRx(node, rxPacket);
}

void psLineInterrupt(tNode * node, bool psVal) {
    nodeTransferCompleteCb(node);
}

// ================================ //

uint8_t * createNodeTxPacketToFillIn(tNode * node, uint16_t packetSize) {
    if (node->numTxPackets < MAX_TX_PACKETS) {
        tPacket * packet = &node->txPacket[node->numTxPackets];
        packet->nodeId = node->nodeId;
        packet->dataSize1 = packetSize >> 8;
        packet->dataSize2 = packetSize & 0xFF;
        node->numTxPackets++;
        return packet;
    } else {
        myAssert(0, "Tx buffer is full");
        return NULL;
    }
}