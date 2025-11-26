// Copyright (c) 2025 Sean Bremner
// Licensed under the MIT License. See LICENSE file for details.

// TODO: what if the master transfer doesn't get enough bytes and therefore doesn't complete

#include "stdlib.h"
#include "string.h"
#include "assert.h"

#include "../src/scheduler.h"
#include "../src/microbus.h"
#include "../src/node.h"
#include "../src/txManager.h"
#include "../src/networkManager.h"

#define MAX_SLOTS_BETWEEN_UNALLOCATED 80

tSchedulerState scheduler;
static tNodeQueue activeNodes = {0};
static tNodeQueue activeTxNodes = {0};
static tNodeQueue nodeTxNodes = {0};

void basicSchedulerInit(uint32_t numActiveNodes, bool withAllocation) {
    activeNodes.numNodes = 0;
    activeNodes.lastIndex = 0;
    activeTxNodes.numNodes = 0;
    activeTxNodes.lastIndex = 0;
    nodeTxNodes.numNodes = 0;
    nodeTxNodes.lastIndex = 0;
    schedulerInit(&scheduler, &activeNodes, &activeTxNodes, &nodeTxNodes, 1, 80);

    for (tNodeIndex nodeId=1; nodeId<numActiveNodes+1; nodeId++) {
        nodeQueueAdd(&activeNodes, nodeId);
    }
    
    // If we are not testing allocation then set
    // the gap between allocation packets to max so
    // as to speed up the process and ensure our bandwidth readings represent 
    // the long term behaviour
    if (!withAllocation) {
        scheduler.unallocatedSlotGap = 80;
    }
}

void test_scheduler_allocation_slots(void) {
    basicSchedulerInit(5, true);

    tNodeIndex nextExpectedNode = FIRST_NODE_ID;
    uint32_t numAllocationSlots = 0;
    
    // Test that there are frequent allocation nodes initially - despite nodes waiting to transmit
    tNodeIndex nodesToTx[2];
    for (uint32_t j=0; j<100; j++) {
        schedulerUpdateAndCalcNextTxNodes(&scheduler, nodesToTx, 0);
        if (nodesToTx[0] == UNALLOCATED_NODE_ID) {
            numAllocationSlots++;
        }
    }
    assert(numAllocationSlots > 48);

    // Then check this backs off to a low number
    for (uint32_t j=0; j<200000; j++) {
        schedulerUpdateAndCalcNextTxNodes(&scheduler, nodesToTx, 0);
    }
    numAllocationSlots = 0;
    for (uint32_t j=0; j<500; j++) {
        schedulerUpdateAndCalcNextTxNodes(&scheduler, nodesToTx, 0);
        if (nodesToTx[0] == UNALLOCATED_NODE_ID) {
            numAllocationSlots++;
        }
    }
    assert(numAllocationSlots >= (500 / MAX_SLOTS_BETWEEN_UNALLOCATED));
    assert(numAllocationSlots <= 1+(500 / MAX_SLOTS_BETWEEN_UNALLOCATED));


    // Check that if a new node is heard it goes back to being frequent
    NEW_NODE_HEARD_UPDATE_SCHEDULER(scheduler);
    numAllocationSlots = 0;
    for (uint32_t j=0; j<100; j++) {
        schedulerUpdateAndCalcNextTxNodes(&scheduler, nodesToTx, 0);
        if (nodesToTx[0] == UNALLOCATED_NODE_ID) {
            numAllocationSlots++;
        }
    }
    assert(numAllocationSlots > 48);
}

void test_scheduler_servicing(void) {
    basicSchedulerInit(5, false);

    tNodeIndex nextExpectedNode = FIRST_NODE_ID;
    
    // Test all nodes get serviced in turn (as there are no nodes waiting to tx)
    tNodeIndex nodesToTx[0];
    for (uint32_t j=0; j<100; j++) {
        schedulerUpdateAndCalcNextTxNodes(&scheduler, nodesToTx, 0);
        tNodeIndex node = nodesToTx[0];
        if (node != UNALLOCATED_NODE_ID) {
            // Check the next node to be scheduled is just going up incrementally
            assert(node == nextExpectedNode);
            nextExpectedNode++;
            if (nextExpectedNode > 5) {
                nextExpectedNode = FIRST_NODE_ID;
            }
        }
    }
}


void test_scheduler_with_N_node_tx(uint32_t numNodes) {
    basicSchedulerInit(numNodes, false);
    
    // Test all nodes get serviced in turn (as there are no nodes waiting to tx)
    uint32_t count[MAX_NODES] = {0};
    tNodeIndex nodesToTx[0];
    for (uint32_t j=0; j<1000; j++) {
        schedulerUpdateAndCalcNextTxNodes(&scheduler, nodesToTx, 0);
        tNodeIndex node = nodesToTx[0];
        count[node]++;
    }

    // 1/80th of the time allocation slots (13)
    // 25% of the time servicing (250)

    // Remaining should be split
    for (uint32_t i=1; i<numNodes+1; i++) {
        assert(count[i] > 700/numNodes);
    }
}

void test_scheduler_with_1_rx_ack(void) {
    basicSchedulerInit(16, false);
    tNodeIndex nodeId = 1;
    nodeQueueAdd(&activeTxNodes, nodeId);

    uint32_t node1Count = 0;
    tNodeIndex nodesToTx[0];
    for (uint32_t j=0; j<1000; j++) {
        schedulerUpdateAndCalcNextTxNodes(&scheduler, nodesToTx, 0);
        if (nodesToTx[0] == nodeId) {
            node1Count++;
        }
    }

    // 1/80th of the time allocation slots
    // 25% of the time servicing ()
    // Remaining should be for node 1 to get an rx ack
    // Except we only use every 1 in 8 (MAX_MASTER_SLOTS_BETWEEN_ACKS)
    assert(node1Count > (1000/8 - 10));
}

void test_scheduler_with_N_rx_ack(uint32_t numNodes) {
    basicSchedulerInit(numNodes, false);

    for (uint32_t i=1; i<numNodes+1; i++) {
        nodeQueueAdd(&activeTxNodes, i);
    }
    
    // Test all nodes get serviced in turn (as there are no nodes waiting to tx)
    uint32_t count[MAX_NODES] = {0};
    tNodeIndex nodesToTx[0];
    for (uint32_t j=0; j<1000; j++) {
        schedulerUpdateAndCalcNextTxNodes(&scheduler, nodesToTx, 0);
        tNodeIndex node = nodesToTx[0];
        count[node]++;
    }

    // 1/80th of the time allocation slots (13)
    // 25% of the time servicing (250)

    // Remaining should be split
    uint32_t expCount = 700/numNodes;
    if (expCount > 245) {
        expCount = 245;
    }
    for (uint32_t i=1; i<numNodes+1; i++) {
        assert(count[i] > expCount);
    }
}




void testScheduler() {
    test_scheduler_allocation_slots();
    test_scheduler_servicing();
    test_scheduler_with_N_node_tx(1);
    test_scheduler_with_N_node_tx(2);
    for (uint32_t i=0; i<100; i++) {
        uint32_t numNodes = (1+ rand()) % MAX_NODES;
        test_scheduler_with_N_node_tx(numNodes);
    }
    test_scheduler_with_1_rx_ack();
    for (uint32_t i=0; i<100; i++) {
        uint32_t numNodes = (1 + rand()) % MAX_NODES;
        test_scheduler_with_N_rx_ack(numNodes);
    }
}

