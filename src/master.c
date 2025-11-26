// Copyright (c) 2025 Sean Bremner
// Licensed under the MIT License. See LICENSE file for details.

#include "string.h"
#include "stdbool.h"
#include "stdint.h"
#include "stdlib.h"

//#include "scheduler.h"
#include "microbus.h"
#include "master.h"
#include "txManager.h"
#include "networkManager.h"
#include "masterRx.h"
#include "masterTx.h"

// ========================================= //
// Update Schedule

static void masterUpdateSchedule(tMaster * master) {
    if (master->tx.masterResetCycles > 0) {
        master->currentTxNodeId = MASTER_NODE_ID;
        for (uint8_t i=1; i<master->scheduler.numTxNodesScheduled+1; i++) {
            master->nextTxNodeId[i] = MASTER_NODE_ID;
        }
        return;
    }

    // Shift schedule for next slot
    master->currentTxNodeId = master->nextTxNodeId[0];
    uint32_t txBufferLevel = getNumAllBufferedTxPackets(&master->tx.txManager);
    schedulerUpdateAndCalcNextTxNodes(&master->scheduler, master->nextTxNodeId, txBufferLevel);
    // MB_PRINTF("Master - schedule %u,%u,%u,%u,%u,%u\n", master->nextTxNodeId[0], master->nextTxNodeId[1], master->nextTxNodeId[2], master->nextTxNodeId[3], master->nextTxNodeId[4], master->nextTxNodeId[5]);
}

// ========================================= //


// Returns numTxPacketsFreed
static uint8_t masterRemoveAnyTimeoutNodes(tMaster * master) {
    uint8_t numTxPacketsFreed = 0;
    for (uint32_t nodeId=FIRST_NODE_ID; nodeId<MAX_NODES; nodeId++) {
        if (master->masterNodeTimeToLive[nodeId] == REMOVE_NODE_TTL) {
            master->masterNodeTimeToLive[nodeId] = 0;
            // Clear all tx packets
            networkManagerRemoveNewNodeRequest(&master->nwManager, nodeId);
            nodeQueueRemoveIfExists(&master->activeNodes, nodeId);
            nodeQueueRemoveIfExists(&master->nodeTxNodes, nodeId);
            masterTxManagerRemoveNode(&master->tx.txManager, nodeId, &numTxPacketsFreed);
            rxManagerRemoveAllPackets(&master->rx.rxPacketManager, nodeId);
            MB_PRINTF("Master - Node:%u, removed from network\n", nodeId);
        }
    }
    return numTxPacketsFreed;
}

// ========================================= //
// Dual channel pipelined
// The pre process is quick before the slot starts packets go out
// The post process is slow and can be run whilst the next packet is 
// being transmitted and received. 
// This is for the main SPI link which needs to run as fast as possible

void masterDualChannelPipelinedPreProcess(tMaster * master, tPacket ** txPacket, tPacket ** rxPacketMemory, bool crcError) {
    masterUpdateSchedule(master);
    // Quick validate rx packet and record the seq nums so we can ack them as soon as possible
    masterQuickProcessPrevRx(&master->rx, &master->nwManager, &master->tx.txManager, master->masterNodeTimeToLive, crcError);
    // Now fill in the updated schedule and the acks
    masterQuickUpdateTxPacket(&master->tx, &master->scheduler, master->nextTxNodeId);
    *txPacket = masterTxGetNextTxPacket(&master->tx);
    *rxPacketMemory = masterRxGetNextPacketMemory(&master->rx);
}

// Return numTxFreed
uint8_t masterDualChannelPipelinedPostProcess(tMaster * master) {
    // Process the recieved frame
    uint8_t numTxFreed = masterProcessRx(&master->rx, &master->nwManager, &master->scheduler, &master->tx.txManager, master->masterNodeTimeToLive);
    // Prepare the next tx frame
    masterProcessTx(&master->tx, &master->nwManager, &master->scheduler, master->nextTxNodeId);
    // Update for the end of slot
    numTxFreed += masterRemoveAnyTimeoutNodes(master);
    return numTxFreed;
}

// ========================================= //
// Single channel - no pipeline
// As it's single channel it's either Tx or Rx - so separate calls
// No pipeline so we can just process everything as it comes in or goes out

tPacket * masterGetRxPacketMemory(tMaster * master) {
    return masterRxGetNextPacketMemory(&master->rx);
}

// return numTxFreed
uint8_t masterNoDelaySingleChannelProcessRx(tMaster * master, bool crcError) {
    // masterProcessTx(&master->tx, &master->nwManager, &master->scheduler, master->nextTxNodeId);
    masterUpdateSchedule(master);
    masterQuickProcessPrevRx(&master->rx, &master->nwManager, &master->tx.txManager, master->masterNodeTimeToLive, crcError);
    uint8_t numTxFreed = masterProcessRx(&master->rx, &master->nwManager, &master->scheduler, &master->tx.txManager, master->masterNodeTimeToLive);
    // numTxFreed += masterRemoveAnyTimeoutNodes(master);
    return numTxFreed;
}

// return numTxFreed
uint8_t masterNoDelaySingleChannelProcessTx(tMaster * master, tPacket ** txPacket) {
    masterProcessTx(&master->tx, &master->nwManager, &master->scheduler, master->nextTxNodeId);
    masterUpdateSchedule(master);
    masterQuickUpdateTxPacket(&master->tx, &master->scheduler, master->nextTxNodeId);
    *txPacket = masterTxGetNextTxPacket(&master->tx);
    return masterRemoveAnyTimeoutNodes(master);
}

// ========================================= //
// Called by any thread
void masterUpdateTimeUs(tMaster * master, uint32_t usIncr) {
    networkManagerUpdateTimeUs(&master->nwManager, master->masterNodeTimeToLive, usIncr);
}

// ========================================= //
// Called by main thread

void masterInit(
        tMaster * master,
        uint8_t numTxNodesScheduled, // max gap between master tx slots -1
        uint8_t maxTxPacketEntries,
        tPacketEntry txPacketEntries[],
        uint8_t maxRxPacketEntries,
        tPacketEntry rxPacketEntries[],
        tPacketEntry * rxPacketQueue[]) {
            
    memset(master, 0, sizeof(tMaster));

    networkManagerInit(&master->nwManager, &master->activeNodes);
    schedulerInit(&master->scheduler, &master->activeNodes, &master->activeTxNodes, &master->nodeTxNodes, numTxNodesScheduled, 80);
    masterRxInit(&master->rx, maxRxPacketEntries, rxPacketEntries, rxPacketQueue, &master->stats);
    masterTxInit(&master->tx, &master->stats, &master->activeTxNodes, maxTxPacketEntries, txPacketEntries);

    // Start by sending reset packets for 20 cycles
    master->tx.masterResetCycles = 20;
}

void getConnectedNodesBitField(void * master, uint8_t connectedNodesBitfield[MAX_NODES/8]) {
    tMaster * rmaster = master;
    rmaster->masterNodeTimeToLive[0] = 1; // Mark our own node as active
    for (uint32_t node=0; node<MAX_NODES; node++) {
        if (rmaster->masterNodeTimeToLive[node] > 0) {
            connectedNodesBitfield[node/8] |= (0x1 << (7 - (node % 8)));
        }
    }
    // Remove any new nodes as they haven't properly joined yet
    for (uint32_t i=0; i<rmaster->nwManager.numNewNodes; i++) {
        uint8_t node = rmaster->nwManager.newNodeId[i];
        microbusAssert(node != 0, "");
        connectedNodesBitfield[node/8] &= ~(0x1 << (7 - (node % 8)));
    }
}

void masterResetTxCredits(void * master) {
    tMaster * rmaster = master;
    masterTxClearBuffers(&rmaster->tx.txManager);
}

uint8_t * masterPeekNextRxDataPacket(void * master, uint16_t * size, tNodeIndex * srcNodeId) {
    tMaster * rmaster = master;
    tPacket * packet = peekNextRxDataPacket(&rmaster->rx.rxPacketManager);
    if (packet == NULL) {
        return NULL;
    }
    *size = GET_PACKET_DATA_SIZE(packet);
    *srcNodeId = packet->node.srcNodeId;
    return packet->node.data;
}

bool masterPopNextDataPacket(void * master) {
    tMaster * rmaster = master;
    return popNextDataPacket(&rmaster->rx.rxPacketManager);
}

uint8_t * masterAllocateTxPacket(void * master) {
    tMaster * rmaster = master;
    tPacket * packet = allocateTxPacket(&rmaster->tx.txManager, MASTER_NODE_ID);
    if (packet == NULL) {
        rmaster->stats.txBufferFull++;
        return NULL;
    }
    return packet->master.data;
}

void masterSubmitAllocatedTxPacket(void * master, tNodeIndex dstNodeId, uint16_t numBytes) {
    tMaster * rmaster = master;
    if (numBytes > MASTER_PACKET_DATA_SIZE) {
        microbusAssert(0, ""); // "Tx packet exceeds max size"
    }
    tPacket * packet = rmaster->tx.txManager.allocatedPacket;
    if (packet == NULL) {
        microbusAssert(0, "");
    }
    submitAllocatedTxPacket(&rmaster->tx.txManager, true, &rmaster->masterNodeTimeToLive[dstNodeId], MASTER_NODE_ID, dstNodeId, MASTER_DATA_PACKET, numBytes);
}


