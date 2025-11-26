// Copyright (c) 2025 Sean Bremner
// Licensed under the MIT License. See LICENSE file for details.

#include "string.h"
#include "stdbool.h"
#include "stdint.h"
#include "stdlib.h"

#include "microbus.h"
#include "masterRx.h"


// Here we want to determine quickly as possible if the rxPacket memory can be re-used 
// or if it needs to be stored. This reduces the size of the receive buffers by 1 packet
void masterQuickProcessPrevRx(tMasterRx * rx, tNetworkManager * nwManager, tTxManager * txManager, uint8_t masterNodeTimeToLive[MAX_NODES], bool rxCrcError) {
    // NOTE: by default we will reuse the current rx packet (unless there is valid rx data)
    rx->prevRxPacketEntry = rx->nextRxPacketEntry;
    tPacket * rxPacket = &rx->prevRxPacketEntry->packet;
    rx->validRxPacket = false;

    // If the version and packet type is 0 then it's probably just an empty packet
    if (rxPacket->protocolVersionAndPacketType == 0 || rxPacket->protocolVersionAndPacketType == 255) {
        rx->stats->emptyRx++;
        return;
    }

    if (rxCrcError) {
        rx->stats->rxCrcFailures++;
        // microbusAssert(0, "");
        return;
    }

    if (GET_PROTOCOL_VERSION(rxPacket) != MICROBUS_VERSION) {
        rx->stats->rxInvalidProtocol++;
        //microbusAssert(0, "");
        return;
    }

    if (GET_PACKET_DATA_SIZE(rxPacket) >= MAX_PACKET_DATA_SIZE) {
        rx->stats->rxInvalidDataSize++;
        //microbusAssert(0, "");
        return;
    }

    rx->stats->rxValid++;
    rx->validRxPacket = true;
    
    // Record we've received a packet - before finding out whether the buffer is full
    tPacketType packetType = GET_PACKET_TYPE(rxPacket);
    if (packetType == NODE_DATA_PACKET || packetType == NODE_EMPTY_PACKET) {
        if (masterNodeTimeToLive[rxPacket->node.srcNodeId] > 0) {
            networkManagerRecordRxPacket(nwManager, masterNodeTimeToLive, rxPacket->node.srcNodeId);
        }
    }
    
    // We will store this packet so use a new one for the next rx packet
    // rx->nextRxPacketEntry->valid = true;
    tPacketEntry * freeEntry = findFreeRxPacket(&rx->rxPacketManager);
    if (freeEntry) {
        rx->nextRxPacketEntry = freeEntry;
    } else {
        rx->stats->rxBufferFull++;
        // Re-use this packet 
        rx->validRxPacket = false;
        //microbusAssert(0, "");
        return;
    }

    // Check sequence number
    // We want to update our sequence number as quickly as possible so we can ack it straight away
    // rx->validRxSeqNum = true;
    if (GET_PACKET_TYPE(rxPacket) == NODE_DATA_PACKET) {
        tNodeIndex srcNodeId = rxPacket->node.srcNodeId;
        rx->validRxSeqNum = rxPacketCheckAndUpdateSeqNum(txManager, srcNodeId, rxPacket->txSeqNum, true);
    }
}

// NOTE: this can't take too long. It must complete before the next packet is transmitted
// Returns numTxPacketsFreed
uint8_t masterProcessRx(tMasterRx * rx, tNetworkManager * nwManager, tSchedulerState * scheduler, tTxManager * txManager, uint8_t masterNodeTimeToLive[MAX_NODES]) {
    if (!rx->validRxPacket) {
        return 0;
    }

    uint8_t numTxPacketsFreed = 0;
    tPacketEntry * rxPacketEntry = rx->prevRxPacketEntry;
    tPacket * rxPacket = &rxPacketEntry->packet;
    tPacketType packetType = GET_PACKET_TYPE(rxPacket);
    bool packetStored = false;
    rx->stats->rxPacketEntries++;

    if (MICROBUS_LOG_PACKETS && MICROBUS_LOGGING) {
        microbusPrintPacket(rxPacket, true, 0, false, scheduler->numTxNodesScheduled);
    }

    switch (packetType) {
        case NODE_EMPTY_PACKET: {
            tNodeIndex srcNodeId = rxPacket->node.srcNodeId;
            // Record the other ends acknowledgement
            numTxPacketsFreed += rxAckSeqNum(txManager, srcNodeId, rxPacket->node.ackSeqNum, true, &rx->stats->txWindowRestarts);
            break;
        }
        case NODE_DATA_PACKET: {
            tNodeIndex srcNodeId = rxPacket->node.srcNodeId;
            // Record the other ends acknowledgement
            numTxPacketsFreed += rxAckSeqNum(txManager, srcNodeId, rxPacket->node.ackSeqNum, true, &rx->stats->txWindowRestarts);
            // If it's the sequence number is expected
            if (rx->validRxSeqNum) {
                microbusAssert(rxPacket->dataSize1 > 0 || rxPacket->dataSize2 > 0, "");
                // Update the nodes buffer level - so the scheduler can schedule it again if the buffer is > 0
                schedulerUpdateNodeTxBufferLevel(scheduler, srcNodeId, rxPacket->node.bufferLevel);
                rx->stats->rxDataPackets++;
                // Add Rx data to queue
                addRxDataPacket(&rx->rxPacketManager, rxPacketEntry);
                packetStored = true;
            }
            break;
        }
        case NEW_NODE_REQUEST_PACKET:
            rxNewNodePacketRequest(nwManager, masterNodeTimeToLive, rxPacket, &rx->stats->networkFullCount);
            rx->stats->newNodeRequestRx++;
            NEW_NODE_HEARD_UPDATE_SCHEDULER((*scheduler));
            break;
        default:
            rx->stats->rxInvalidPacketType++;
            //microbusAssert(0, "");
            break;
    }
    if (packetStored == false) {
        // Free the packet
        rxPacketEntry->inUse = false;
        // rx->nextRxPacketEntry->valid = false;
    }
    
    rx->validRxPacket = false;
    rx->validRxSeqNum = false;
    return numTxPacketsFreed;
}

tPacket * masterRxGetNextPacketMemory(tMasterRx * rx) {
    return &rx->nextRxPacketEntry->packet;
}

void masterRxInit(tMasterRx * rx, uint8_t maxRxPacketEntries, tPacketEntry rxPacketEntries[], tPacketEntry * rxPacketQueue[], tNodeStats * stats) {
    microbusAssert(maxRxPacketEntries > 2, "");
    rxManagerInit(&rx->rxPacketManager, maxRxPacketEntries, rxPacketEntries, rxPacketQueue);
    rx->nextRxPacketEntry = findFreeRxPacket(&rx->rxPacketManager);
    microbusAssert(rx->nextRxPacketEntry, "");
    rx->stats = stats;
}


