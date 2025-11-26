// Copyright (c) 2025 Sean Bremner
// Licensed under the MIT License. See LICENSE file for details.

#include "string.h"
#include "stdbool.h"
#include "stdint.h"
#include "stdlib.h"

#include "microbus.h"
#include "master.h"
#include "txManager.h"
#include "networkManager.h"

void masterQuickUpdateTxPacket(tMasterTx * tx, tSchedulerState * scheduler, tNodeIndex nextTxNodeId[MAX_TX_NODES_SCHEDULED]) {
    if (tx->nextTxPacket) {
        
        // Update the outgoing Tx's packet with new schedule and acks
        for (uint8_t i=0; i<scheduler->numTxNodesScheduled; i++) {
            tNodeIndex nodeId = nextTxNodeId[i]; // There is a delay of 1
            tx->nextTxPacket->master.nextTxNodeId[i] = nodeId;
            tx->nextTxPacket->master.nextTxNodeAckSeqNum[i] = tx->txManager.rxSeqNum[nodeId];
        }

        tx->stats->txPackets++;
        if (MICROBUS_LOG_PACKETS && MICROBUS_LOGGING) {
            microbusPrintPacket(tx->nextTxPacket, true, 0, true, scheduler->numTxNodesScheduled);
        }
    }
}

// Work out the next tx packet
void masterProcessTx(tMasterTx * tx, tNetworkManager * nwManager, tSchedulerState * scheduler, tNodeIndex nextTxNodeId[MAX_TX_NODES_SCHEDULED]) {
    tPacket * txPacket = NULL;

    if (tx->masterResetCycles > 0) {
        tx->masterResetCycles--;
        txPacket = &tx->tmpPacket;
        SET_PROTOCOL_VERSION_AND_PACKET_TYPE(txPacket, MASTER_RESET_PACKET);
        MB_PRINTF("Master reset cycle\n");
    } else {
        // if ((nextTxNodeId[0] == MASTER_NODE_ID) || (scheduler->numTxNodesScheduled == 1)) {
            // Always respond to new node requests before sending normal data packets

            // And only allow new node responses every other cycle
            // This is because the packet memory is only DMA'd a cycle later
            tx->tmpPacketCycle++;
            if (tx->tmpPacketCycle >= 4) {
                tx->tmpPacketCycle = 0;
            }
            
            // Only send new node responses every Nth cycle (so it doesn't consume all the bandwidth when nodes are joining)
            if ((nwManager->numNewNodes > 0) && (tx->tmpPacketCycle == 1)) {
                txPacket = &tx->tmpPacket;
                tx->stats->newNodeAllocated++;
                txNewNodeResponse(nwManager, txPacket);
                MB_TX_MANAGER_PRINTF("Master Prepare Tx new node\n");

            } else {
                // TODO: issue where we only get <50% bandwidth when single channel, nodes>1 and only master sending data
                //       because we continually schedule acks. We could get 75% if we sent bursts of up to 3 and only
                //       scheduled acks every 4
                uint8_t burstSize = 1; //scheduler->numTxNodesScheduled > 1 ? 3 : 1;
                txPacket = masterGetNextTxDataPacket(&tx->txManager, scheduler->numTxNodesScheduled, nextTxNodeId, burstSize);

                // Alternate between 2 empty packet headers
                // If no valid packet to send then send a blank one
                // In theory we don't have to send a pack every turn we can send it every so often
                // but sending it every turn means the behavoiur is worst case so it helps test the system 
                // If we want to be more efficient in the future we can change this
                if (txPacket == NULL) {
                    tPacketHeader * txPacketHeader = &tx->tmpEmptyPacketHeader[tx->tmpPacketCycle % 2];
                    SET_PROTOCOL_VERSION_AND_PACKET_TYPE(txPacketHeader, MASTER_EMPTY_PACKET);
                    txPacketHeader->dataSize1 = 0;
                    txPacketHeader->dataSize2 = 0;
                    txPacketHeader->master.dstNodeId = INVALID_NODE_ID;
                    txPacket = (tPacket *)txPacketHeader; // A bit hacky - the DMA will access a few hundred bytes beyond the packet header
                    // MB_TX_MANAGER_PRINTF("Master Prepare Tx Empty packet\n");
                } else {
                    microbusAssert(GET_PACKET_DATA_SIZE(txPacket) < MAX_PACKET_DATA_SIZE, "");
                    tx->stats->txDataPackets++;
                }
            }
        // }
    }
    tx->nextTxPacket = txPacket;
}

tPacket * masterTxGetNextTxPacket(tMasterTx * tx) {
    return tx->nextTxPacket;
}

void masterTxInit(tMasterTx * tx, tNodeStats * stats, tNodeQueue * activeTxNodes, uint8_t maxTxPacketEntries, tPacketEntry txPacketEntries[]) {
    initTxManager(
        &tx->txManager,
        MAX_NODES,
        tx->txManagerMemory.txSeqNumStart,
        tx->txManagerMemory.txSeqNumEnd,
        tx->txManagerMemory.txSeqNumNext,
        tx->txManagerMemory.txSeqNumPauseCount,
        tx->txManagerMemory.rxSeqNum,
        activeTxNodes,
        maxTxPacketEntries,
        txPacketEntries
    );

    tx->stats = stats;

    // Start with a valid packet
    tx->nextTxPacket = &tx->tmpPacket;
    tx->nextTxPacket->protocolVersionAndPacketType = MASTER_EMPTY_PACKET | (MICROBUS_VERSION << 4);
    tx->nextTxPacket->dataSize1 = 0;
    tx->nextTxPacket->dataSize2 = 0;
    tx->nextTxPacket->master.dstNodeId = INVALID_NODE_ID;
}

