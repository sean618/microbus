// Copyright (c) 2025 Sean Bremner
// Licensed under the MIT License. See LICENSE file for details.

#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "stdint.h"
#include "microbus.h"
#include "txManager.h"

#define MAX_MASTER_SLOTS_BETWEEN_ACKS 4 // Needs to match the tx queue size (otherwise cannot fit in more tx packets till the ack comes back)
#define MAX_SLOTS_BETWEEN_SERVICING 6
#define MIN_SLOTS_BETWEEN_UNALLOCATED 2
#define NUM_SLOTS_BEFORE_ALLOCATION_CHANGED 128

#define FOREACH_TURN_ENUM(APPLY_MACRO) \
    APPLY_MACRO(MASTER_TX) \
    APPLY_MACRO(MASTER_RX_ACK) \
    APPLY_MACRO(NODE_TX) \
    APPLY_MACRO(MAX_TURN)

#define GENERATE_ENUM(ENUM) ENUM,
#define GENERATE_STRING(STRING) #STRING,

typedef enum {
    FOREACH_TURN_ENUM(GENERATE_ENUM)
} eSchedulerTurn;

#if MICROBUS_LOGGING > 0
static const char *TURN_ENUM_STRING[] = {
    FOREACH_TURN_ENUM(GENERATE_STRING)
};
#endif


// Master only
typedef struct {
    // uint16_t masterTxBufferLevel;
    uint8_t numTxNodesScheduled;
    uint8_t maxSlotsBetweenUnallocated;
    uint8_t unallocatedSlotGapUpdateCount;
    uint8_t unallocatedSlotGap;
    uint8_t countTillNextAllocation;
    uint8_t countTillNextService;
    uint8_t rxAckEndCount;
    eSchedulerTurn nextTurn;
    tNodeIndex nextServiceNode;
    tNodeIndex nextMasterRxAckNode;
    tNodeIndex nextNodeTxNode;
    tNodeIndex recentScheduledNodes[MAX_MASTER_SLOTS_BETWEEN_ACKS];
    uint8_t nodeTxBufferLevel[MAX_NODES];
    tNodeQueue * activeNodes;   // All connected nodes
    tNodeQueue * activeTxNodes; // Nodes we have sent data to and are waiting for an ack
    tNodeQueue * nodeTxNodes;   // Nodes that currently have tx packets buffered waiting to go out
} tSchedulerState; // ~112 bytes

void schedulerInit(tSchedulerState * scheduler, tNodeQueue * activeNodes, tNodeQueue * activeTxNodes, tNodeQueue * nodeTxNodes, uint8_t numTxNodesScheduled, uint8_t maxSlotsBetweenUnallocated);
void schedulerUpdateAndCalcNextTxNodes(tSchedulerState * scheduler, tNodeIndex nodesToTx[MAX_TX_NODES_SCHEDULED], uint8_t masterTxBufferLevel);
void schedulerUpdateNodeTxBufferLevel(tSchedulerState * scheduler, tNodeIndex srcNodeId, uint8_t bufferLevel);

// If there are new nodes on the bus reset the gap between unallocated slots to minimum
// to allocate all nodes as quickly as possible
// Once there are no more new nodes this can increase again
#define NEW_NODE_HEARD_UPDATE_SCHEDULER(scheduler)  { \
    (scheduler).unallocatedSlotGap = MIN_SLOTS_BETWEEN_UNALLOCATED; \
    (scheduler).countTillNextAllocation = MIN_SLOTS_BETWEEN_UNALLOCATED; \
}

#endif