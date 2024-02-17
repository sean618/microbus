#include "string.h"
#include "stdbool.h"
#include "stdint.h"
#include "stdio.h"
#include "stdlib.h"
//#include "scheduler.h"
#include "microbus.h"
#include "master.h"

void halSetPs(tMasterNode * master, bool val);
void halStartMasterTxRxDMA(tMasterNode * master, uint8_t * txData, uint8_t * rxData, uint32_t numBytes);

// ================================= //
// New node request & response

void rxNewNodePacketRequest(tMasterNode * node, tPacket * packet) {
    for (uint32_t i=0; i<PACKET_DATA_SIZE; i+=NEW_NODE_REQUEST_ENTRY_SIZE) {
        uint64_t newNodeUniqueId;
        uint16_t rxCheckSum;
        memcpy(&newNodeUniqueId, &packet->data[i],   8);
        memcpy(&rxCheckSum,      &packet->data[i+8], 2);
        uint16_t checkSum = (0xFFFF & (newNodeUniqueId >> 48)) 
                            + (0xFFFF & (newNodeUniqueId >> 32)) 
                            + (0xFFFF & (newNodeUniqueId >> 16)) 
                            + (0xFFFF & (newNodeUniqueId));
        if (checkSum == rxCheckSum) {
            masterSchedulerAddNewNode(&node->scheduler, newNodeUniqueId);
        }
    }
}


tPacket * txNewNodeResponse(tMasterNode * node) {
    tPacket * packet = &node->tmpPacket;
    packet->protocolVersionAndPacketType = (MICROBUS_VERSION & 0xF) << 4 | (uint8_t) NEW_NODE_RESPONSE_PACKET;
    uint16_t num = numNewNode(node->scheduler);
    num = MIN(num, PACKET_DATA_SIZE / NEW_NODE_RESPONSE_ENTRY_SIZE);
    for (uint16_t i=0; i<num; i++) {
        uint64_t uniqueId;
        uint8_t nodeId;
        popNewNode(node->scheduler, &uniqueId, &nodeId);
        memcpy(&packet->data[i*NEW_NODE_RESPONSE_ENTRY_SIZE], &uniqueId, 8);
        packet->data[8 + i*NEW_NODE_RESPONSE_ENTRY_SIZE] = nodeId;
    }
    //packet->srcNodeId = INVALID_NODE_ID;
    packet->dstNodeId = UNALLOCATED_NODE_ID;
    uint16_t dataSize = num * NEW_NODE_RESPONSE_ENTRY_SIZE;
    packet->dataSize1 = dataSize >> 8;
    packet->dataSize2 = dataSize & 0xFF;
    return packet;
}

// ================================= //

tPacket * allocateRxPacket(tNode * node) {
    if (CIRCULAR_BUFFER_FULL(node->rxPacketsStart, node->rxPacketsEnd, node->numRxPackets)) {
        // A bit of a hack - the circular buffer implementation always leaves an extra entry
        // as it regards start == end to be empty rather than full
        // We need an extra buffer to process the rx packet meta data if the normal buffer is full
        // so use this extra one
        return &node->rxPackets[node->rxPacketsEnd];
        
    } else {
        tPacket * rxPacket;
        CIRCULAR_BUFFER_ALLOCATE(rxPacket, node->rxPacket, node->rxPacketsStart, node->rxPacketsEnd, node->numRxPackets);
        return rxPacket;
    }
}

// void masterRxDataPacket(tMasterNode * node, tPacket * packet) {
//     uint16_t dataSize = packet->dataSize1 << 8 || packet->dataSize2;
//     if (packet->nodeId != INVALID_NODE_ID && dataSize > 0) { 
//         //MB_PRINTF(node, "Node rx:%d\n", node->rxPacket[node->rxPacketsReceived].data[0]);
//         myAssert(node->rxPacketsReceived < MAX_RX_PACKETS, "Rx packet buffer overflow");
//         node->rxPacketsReceived++;
//     }
// }

void masterProcessRx(tMasterNode * node) {
    tPacket * packet = &node->rxPacket[node->rxPacketsReceived];


    uint8_t protocolVersion = (packet->protocolVersionAndPacketType >> 4) & 0xF;
    tPacketType packetType = packet->protocolVersionAndPacketType & 0xF;

    // 0xFF means nothing was sent (as the line is pulled high)
    if (packetType == 0xFF) {
        return;
    }
    if (protocolVersion != MICROBUS_VERSION) {
        myAssert(0, "Invalid version");
        return;
    }

    if (packetType == NEW_NODE_REQUEST_PACKET) {
        rxNewNodePacketRequest(node, packet);
    } else if (packetType == DATA_PACKET) {
        masterRxDataPacket(node, packet);
    } else {
        myAssert(0, "Master received invalid packet type during DATA_MODE");
    }
}

tPacket * masterTxDataPacket(tMasterNode * node) {
    tPacket * packet = &node->txPacket[node->txPacketsSent];
    packet->protocolVersionAndPacketType = (MICROBUS_VERSION & 0xF) << 4 | (uint8_t) DATA_PACKET;
    //packet->seqNum = ;
    node->txPacketsSent++;
    myAssert(node->txPacketsSent < MAX_TX_PACKETS, "Sent more tx packets than buffer size");
    //MB_PRINTF(node, "Master %d: txSlot:%d, sending\n", node->simId, node->currentSlot, node->txPacketsSent, node->numTxPackets);
    return packet;
}

tPacket * masterProcessTx(tMasterNode * node) {
    if (numNewNode(node->scheduler) > 0) {
        return txNewNodeResponse(node);
    } else if (node->txPacketsSent < node->numTxPackets) {
        return txNewNodeResponse(node);
    } else {
        tPacket * packet = &node->tmpPacket;
        packet->protocolVersionAndPacketType = (MICROBUS_VERSION & 0xF) << 4 | (uint8_t) EMPTY_PACKET;
        packet->nodeId = INVALID_NODE_ID;
        packet->dataSize1 = 0;
        packet->dataSize2 = 0;
        return packet;
    }
}

void masterTransferCompleteCb(tMasterNode * master, bool startSequence) {
    tNodeIndex nextTxNodeId = masterSchedulerGetNextTxNodeId();
    
    processRxHeaders(node, &node->rxPackets[node->rxPacketsEnd]);

    tPacket * txPacket = masterProcessTx(master);
    txPacket->nextTxNodeId = nextTxNodeId;
    master->tx = true;
    // #ifdef LOGGING
    //     MB_PRINTF(master, "Master: mode:%u, slot:%u, numSlots:%u\n", master->mode, master->currentSlot, );
    // #endif
    
    // Master needs to tell nodes to start their DMA
    master->psVal = !master->psVal;
    halSetPs(master, master->psVal);
    
    // Wait 20us
    tPacket * rxPacket = allocateRxPacket(node);
    
    myAssert(master->rxPacketsReceived < MAX_RX_PACKETS, "Received more rx packets than buffer size");
    halStartMasterTxRxDMA(master, txPacket->data, (uint8_t *) &rxPacket, PACKET_SIZE);
    
    masterProcessRx(master);
}

// ================================ //

void initMaster(tMasterNode * master, ) {

}

uint8_t * createMasterTxPacketToFillIn(tMasterNode * master, tNodeIndex dstNodeId, uint16_t packetSize) {
    if (master->numTxPackets < MAX_TX_PACKETS) {
        // Find free entry
        for (uint32_t i=0; i<MAX_TX_PACKETS; i++) {
            tTxPacketEntry * packetEntry = &master->txPacket[i];
            if (!packetEntry->valid) {
                packetEntry->orderNum = master->startOrderNum + master->numTxPackets;
                master->numTxPackets++; // TODO: is this thread safe
                tPacket * packet = packetEntry->packet;
                packet->nodeId = dstNodeId;
                packet->dataSize1 = packetSize >> 8;
                packet->dataSize2 = packetSize & 0xFF;
                master->numTxPackets++;
                packetEntry->valid = true; // Set at the end to ensure thread safe
                return packet;

            }
        }
        myAssert(0, "No free tx packets - something went wrong");

    } else {
        myAssert(0, "Tx buffer is full");
        return NULL;
    }
}

void startMicrobus(tMasterNode * master) {
    masterTransferCompleteCb(master, true);
}


