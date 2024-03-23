#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "stdint.h"
#include "microbus.h"

#define NUM_NODES_DIVIDED_BY_32 ((MAX_NODES + 31)/32)

// Master only
typedef struct {
    uint8_t nodeTxBufferLevel[MAX_NODES];
    uint16_t slotsSinceLastScheduled[MAX_NODES];
} tSchedulerState;

void schedulerUpdateAndCalcNextTxNodes(tSchedulerState * scheduler, uint8_t * nodeTTL, uint8_t maxRxNodes, tNodeIndex nodesToTx[NUM_TX_NODES_SCHEDULED]);

#endif