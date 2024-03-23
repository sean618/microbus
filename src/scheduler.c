#include "string.h"
#include "stdbool.h"
#include "stdint.h"
#include "stdio.h"

#include "scheduler.h"
//#include "cexception.h"
#include "microbus.h"

// =============================================================== //
//                        Scheduler
// 
// This controls who gets the next tx slot/s. This implementation is 
// trying to be simple whilst still giving bandwidth to nodes that 
// need it and also making sure every node gets a chance within a
// certain time.
//
// This is done by first prioritising any node that has gone over it's
// time limit / latency requirement. Then it prioritises any node with
// the highest buffer level (backlog). Finally once all nodes have the
// same buffer level it prioritises the one's that haven't transmitted
// for the longest.
//
// =============================================================== //


static void insertIntoArray(
                uint8_t index, 
                tNodeIndex nodeId, 
                uint8_t nodeBufferLevel,
                uint8_t nodeSlotsSinceLastScheduled,
                tNodeIndex nodesToTx[NUM_TX_NODES_SCHEDULED],
                uint8_t entryBufferLevel[NUM_TX_NODES_SCHEDULED],
                uint8_t entrySlotsSinceLastScheduled[NUM_TX_NODES_SCHEDULED]) {
    // Shift list down
    for (int32_t z=NUM_TX_NODES_SCHEDULED-1; z>index; z--) {
        nodesToTx[z] = nodesToTx[z-1];
        entryBufferLevel[z] = entryBufferLevel[z-1];
        entrySlotsSinceLastScheduled[z] = entrySlotsSinceLastScheduled[z-1];
    }
    nodesToTx[index] = nodeId;
    entryBufferLevel[index] = nodeBufferLevel;
    entrySlotsSinceLastScheduled[index] = nodeSlotsSinceLastScheduled;
}

// Very simple scheduler
// Any node that hasn't had a chance to transmit for more than the threshold gets priority
// After that it's the nodes that have the highest buffer level
// After that it's
void schedulerUpdateAndCalcNextTxNodes(tSchedulerState * scheduler, uint8_t * nodeTTL, uint8_t maxRxNodes, tNodeIndex nodesToTx[NUM_TX_NODES_SCHEDULED]) {
    for (uint8_t i=0; i<NUM_TX_NODES_SCHEDULED; i++) {
        nodesToTx[i] = INVALID_NODE_ID;
    }
    
    uint8_t numNodesExceededLatency = 0;
    uint8_t entryBufferLevel[NUM_TX_NODES_SCHEDULED] = {0};
    uint8_t entrySlotsSinceLastScheduled[NUM_TX_NODES_SCHEDULED] = {0};
    
    // If dual channel then the master has a separate channel so it doesn't need to be scheduled
    tNodeIndex startNode = DUAL_CHANNEL_MODE ? FIRST_AGENT_NODE_ID : 0;
    
    for (tNodeIndex nodeId=startNode; nodeId<maxRxNodes; nodeId++) {
        if (nodeTTL[nodeId] > 0) {
            // Update all slotsSinceLastScheduled counts
            scheduler->slotsSinceLastScheduled[nodeId] += NUM_TX_NODES_SCHEDULED;
            
            uint8_t bufferLevel = scheduler->nodeTxBufferLevel[nodeId];
            uint16_t slotsSinceLastScheduled = scheduler->slotsSinceLastScheduled[nodeId];
            // If a node has exceed it's allowed limit since it's last schedule
            // then immediately add it to the list
            if (slotsSinceLastScheduled >= MAX_SLOTS_PER_NODE_TX_CHANCE) {
                insertIntoArray(numNodesExceededLatency, nodeId, bufferLevel, slotsSinceLastScheduled, nodesToTx, entryBufferLevel, entrySlotsSinceLastScheduled);
                numNodesExceededLatency++;
                // Stop if we have enough that have exceeded their latency
                // (Doesn't matter if there are some that haven't been reached yet that have a higher exceeded amount
                // as this shouldn't actually occur)
                if (numNodesExceededLatency >= NUM_TX_NODES_SCHEDULED) {
                    break;
                }
            } else {
                // Prioritise the nodes that have the highest buffer levels
                // (and if they are the same then the one's that have waited the longest)
                for (int8_t j=numNodesExceededLatency; j<NUM_TX_NODES_SCHEDULED; j++) {
                    //tNodeIndex entryNodeId = nodesToTx[j];
                    bool nodeHasWaitedLonger = slotsSinceLastScheduled > entrySlotsSinceLastScheduled[j];
                    
                    if ((bufferLevel > entryBufferLevel[j]) || ((bufferLevel == entryBufferLevel[j]) && nodeHasWaitedLonger)) {
                        insertIntoArray(j, nodeId, bufferLevel, slotsSinceLastScheduled, nodesToTx, entryBufferLevel, entrySlotsSinceLastScheduled);
                    }
                }
            }
        }
    }
    
    // Now scheduler enough slots to level out the buffers
    uint8_t i=numNodesExceededLatency;
    while (i < NUM_TX_NODES_SCHEDULED-1) {
        // If this node's buffer level is greater than the next then give this node that much more chances to
        // transmit (and shift down the rest of the buffer)
        for (uint8_t j=0; j<(entryBufferLevel[i] - entryBufferLevel[i+1]); j++) {
            insertIntoArray(i+1, nodesToTx[i], entryBufferLevel[i], entrySlotsSinceLastScheduled[i], nodesToTx, entryBufferLevel, entrySlotsSinceLastScheduled);
            i++;
            if (i >= NUM_TX_NODES_SCHEDULED) {
                break;
            }
        }
        i++;
    }
    
    // Zero the counts for the nodes scheduled and reduce the buffer level
    for (int8_t j=0; j<NUM_TX_NODES_SCHEDULED; j++) {
        tNodeIndex nodeId = nodesToTx[j];
        if (nodeId != INVALID_NODE_ID) {
            scheduler->slotsSinceLastScheduled[nodeId] = 0;
            if (scheduler->nodeTxBufferLevel[nodeId] > 0) {
                scheduler->nodeTxBufferLevel[nodeId]--;
            }
        }
    }
}


