// Copyright (c) 2025 Sean Bremner
// Licensed under the MIT License. See LICENSE file for details.

#include "string.h"
#include "stdbool.h"
#include "stdint.h"
#include "stdio.h"

#include "scheduler.h"
#include "microbus.h"

// =============================================================== //
//                        Scheduler
// =============================================================== //

// NOTE! Typically with a sliding window protocol you wait for a bit when reaching the end of the window
// As we normally get acks back quickly we don't bother to wait (as this is more complex) we just go back to the start

static bool nodeRecentlySent(tSchedulerState * scheduler, tNodeIndex node) {
    for (uint32_t i=0; i<MAX_MASTER_SLOTS_BETWEEN_ACKS-1; i++) {
        if (node == scheduler->recentScheduledNodes[i]) {
            return true;
        }
    }
    return false;
}

static tNodeIndex getNextMasterRxAckNode(tSchedulerState * scheduler) {
    if (scheduler->activeTxNodes != NULL) {
        // // When we reach the end pause for a few cycles so that 
        // // we don't schedule it too often for only 1 or 2 nodes
        // if (scheduler->rxAckEndCount > 0) {
        //     scheduler->rxAckEndCount--;
        //     return INVALID_NODE_ID;
        // }
        for (uint32_t i=0; i<scheduler->activeTxNodes->numNodes; i++) {
            // if (queueReachedEnd(scheduler->activeTxNodes)) {
            //     scheduler->rxAckEndCount = 3;
            //     return INVALID_NODE_ID;
            // }
            tNodeIndex nodeId = getNextNodeInQueue(scheduler->activeTxNodes);
            if (!nodeRecentlySent(scheduler, nodeId)) {
                return nodeId;
            }
            // Should only ever have to iterate a maximum of this many 
            microbusAssert(i < MAX_MASTER_SLOTS_BETWEEN_ACKS, "");
        }
    }
    return INVALID_NODE_ID;
}

static tNodeIndex getNextNodeTxNode(tSchedulerState * scheduler) {
    if (scheduler->nodeTxNodes->numNodes > 0) {
        return getNextNodeInQueue(scheduler->nodeTxNodes);
    }
    return INVALID_NODE_ID;
}

static tNodeIndex getNextServiceNode(tSchedulerState * scheduler, bool avoidRecentlyScheduled) {
    scheduler->countTillNextService = MAX_SLOTS_BETWEEN_SERVICING;
    tNodeIndex nodeId = UNALLOCATED_NODE_ID; // If nothing to service it means there are no nodes yet - so send an unallocated node id
    if (scheduler->activeNodes->numNodes > 0) {
        nodeId = getNextNodeInQueue(scheduler->activeNodes);
        if (avoidRecentlyScheduled) {
            if (!nodeRecentlySent(scheduler, nodeId)) {
                MB_SCHEDULER_PRINTF("Master scheduling SERVICE_NODE, node:%u\n", nodeId);
                return nodeId;
            }
        } else {
            MB_SCHEDULER_PRINTF("Master scheduling SERVICE_NODE, node:%u\n", nodeId);
            return nodeId;
        }
    }
    return INVALID_NODE_ID;
}


static tNodeIndex scheduleNextAllocatedNode(tSchedulerState * scheduler, uint8_t masterTxBufferLevel) {
    tNodeIndex node = INVALID_NODE_ID;
    
    if (scheduler->countTillNextService == 0) {
        node = getNextServiceNode(scheduler, true);
        if (node != INVALID_NODE_ID) {
            return node;
        }
    }
    
    // Whichever turn it is (masterRxAck or nodeTx) try and see if there is a candidate
    // if not give the other one a try
    for (uint8_t i=0; i<MAX_TURN; i++) {
        switch (scheduler->nextTurn) {
            case MASTER_TX:
                // Only for dual mode channel
                if (scheduler->numTxNodesScheduled > 1) {
                    if (masterTxBufferLevel > 0) {
                        node = MASTER_NODE_ID;
                        masterTxBufferLevel--;
                    }
                }
                break;
            case MASTER_RX_ACK:
                node = getNextMasterRxAckNode(scheduler);
                break;
            case NODE_TX:
                node = getNextNodeTxNode(scheduler);
                break;
            default:
                microbusAssert(0, "");
        }
        // Alternate between the different modes - giving each a chance
		#if (MICROBUS_LOG_SCHEDULER > 0)
        	eSchedulerTurn turn = scheduler->nextTurn;
		#endif
        scheduler->nextTurn++;
        if (scheduler->nextTurn == MAX_TURN) {
            scheduler->nextTurn = 0;
        }
        if (node != INVALID_NODE_ID) {
            #if (MICROBUS_LOG_SCHEDULER > 0)
                MB_SCHEDULER_PRINTF("Master scheduling %s, node:%u\n", TURN_ENUM_STRING[turn], node);
            #endif
            return node;
        }
    }
    // If no slot is needed for masterRxAck or nodeTx then use it for servicing
    return getNextServiceNode(scheduler, false);
}

tNodeIndex scheduleNextNode(tSchedulerState * scheduler, uint8_t masterTxBufferLevel) {
    tNodeIndex node;
    if (scheduler->countTillNextAllocation == 0) {
        MB_SCHEDULER_PRINTF("Master scheduling ALLOCATION\n");
        // "Pause" any other scheduling whilst we schedule an unallocated slot
        // for any new nodes to join in
        node = UNALLOCATED_NODE_ID;

        // If we've heard a new node since the last unallocated slot then
        // the gap between will be reset to 2. Then after 128
        // unallocated slots if we haven't heard a new node packet
        // double the gap. This ensures we allocate fast but then it
        // doesn't effect normal operation
        if (scheduler->unallocatedSlotGap != scheduler->maxSlotsBetweenUnallocated) {
            scheduler->unallocatedSlotGapUpdateCount++;
            if (scheduler->unallocatedSlotGapUpdateCount == NUM_SLOTS_BEFORE_ALLOCATION_CHANGED) {
                scheduler->unallocatedSlotGap *= 2;
                if (scheduler->unallocatedSlotGap > scheduler->maxSlotsBetweenUnallocated) {
                    scheduler->unallocatedSlotGap = scheduler->maxSlotsBetweenUnallocated;
                }
                MB_SCHEDULER_PRINTF("Master increasing allocation gap: %u\n", scheduler->unallocatedSlotGap);
            }
        }
        scheduler->countTillNextAllocation = scheduler->unallocatedSlotGap-1;
    } else {
        node = scheduleNextAllocatedNode(scheduler, masterTxBufferLevel);
        // Update for the next slot
        microbusAssert(scheduler->countTillNextAllocation > 0, "");
        microbusAssert(scheduler->countTillNextService > 0, "");
        scheduler->countTillNextAllocation--;
        scheduler->countTillNextService--;
        // Shift down the last scheduled nodes and record the new one
        for (uint32_t i=0; i<MAX_MASTER_SLOTS_BETWEEN_ACKS-1; i++) {
            scheduler->recentScheduledNodes[i] = scheduler->recentScheduledNodes[i+1];
        }
        scheduler->recentScheduledNodes[MAX_MASTER_SLOTS_BETWEEN_ACKS-1] = node;
    }
    if (node == INVALID_NODE_ID) {
        node = UNALLOCATED_NODE_ID;
    }
    return node;
}

void schedulerUpdateAndCalcNextTxNodes(tSchedulerState * scheduler, tNodeIndex nodesToTx[MAX_TX_NODES_SCHEDULED], uint8_t masterTxBufferLevel) {
    // Work out the next node (at position scheduler->numTxNodesScheduled - as we're alway generate 1 extra for the delay)
    if (scheduler->numTxNodesScheduled == 1) {
        nodesToTx[0] = scheduleNextNode(scheduler, 0);
    } else {
        bool oneMasterTx = false;

        // Shift all nodes down (and check if the master is currently scheduled)
        for (uint8_t i=0; i<scheduler->numTxNodesScheduled-1; i++) {
            nodesToTx[i] = nodesToTx[i+1];
            if (nodesToTx[i] == MASTER_NODE_ID) {
                if (masterTxBufferLevel > 0) {
                    masterTxBufferLevel--;
                }
            }
            // Work out if we need to send a master packet
            if (nodesToTx[i] == MASTER_NODE_ID) {
                oneMasterTx = true;
            }
        }

        // Schedule the last node
        if (oneMasterTx) {
            // Schedule the next node (add on to the end)
            nodesToTx[scheduler->numTxNodesScheduled-1] = scheduleNextNode(scheduler, masterTxBufferLevel);
        } else {
            // If the master is not currently scheduled then it needs to be 
            // to ensure the next schedule gets sent out
            nodesToTx[scheduler->numTxNodesScheduled-1] = MASTER_NODE_ID;
        }
    }
}

void schedulerUpdateNodeTxBufferLevel(tSchedulerState * scheduler, tNodeIndex srcNodeId, uint8_t bufferLevel) {
    // Record if the node has more packets it wants to send
    if (scheduler->nodeTxBufferLevel[srcNodeId] == 0 && bufferLevel > 0) {
        // Buffer level 0 -> 1
        nodeQueueAdd(scheduler->nodeTxNodes, srcNodeId);
    } else if (scheduler->nodeTxBufferLevel[srcNodeId] > 0 && bufferLevel == 0) {
        // Buffer level 1 -> 0
        nodeQueueRemove(scheduler->nodeTxNodes, srcNodeId);
    }
    scheduler->nodeTxBufferLevel[srcNodeId] = bufferLevel;
}

void schedulerInit(tSchedulerState * scheduler, tNodeQueue * activeNodes, tNodeQueue * activeTxNodes, tNodeQueue * nodeTxNodes, uint8_t numTxNodesScheduled, uint8_t maxSlotsBetweenUnallocated) {
    memset(scheduler, 0, sizeof(tSchedulerState));
    NEW_NODE_HEARD_UPDATE_SCHEDULER(*scheduler);
    scheduler->nextMasterRxAckNode = FIRST_NODE_ID;
    scheduler->nextNodeTxNode = FIRST_NODE_ID;
    scheduler->nextServiceNode = FIRST_NODE_ID;
    scheduler->activeNodes = activeNodes;
    scheduler->activeTxNodes = activeTxNodes;
    scheduler->nodeTxNodes = nodeTxNodes;
    scheduler->maxSlotsBetweenUnallocated = maxSlotsBetweenUnallocated;
    scheduler->numTxNodesScheduled = numTxNodesScheduled;
}
