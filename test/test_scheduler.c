// TODO: what if the master transfer doesn't get enough bytes and therefore doesn't complete

#include "stdlib.h"
#include "unity.h"
#include "microbus.h"
#include "node.h"
#include "scheduler.h"


void test_scheduler_5_equal_nodes(void) {
    tNodeIndex nodesToTx[NUM_TX_NODES_SCHEDULED];
    tSchedulerState scheduler = {0};
    uint8_t nodeTTL[MAX_NODES] = {0};
    // Create 5 nodes - First node is master
    for (uint32_t i=1; i<6; i++) {
        nodeTTL[i] = 0xFF;
    }
    
    // First test 5 equal nodes
    for (uint32_t j=0; j<100; j++) {
        for (uint32_t i=1; i<6; i++) {
            schedulerUpdateAndCalcNextTxNodes(&scheduler, nodeTTL, MAX_NODES, nodesToTx);
            // Check the next node to be scheduled is just going up incrementally
            TEST_ASSERT_EQUAL_UINT(i, nodesToTx[0]);
        }
    }
    
}

void test_scheduler_with_different_ages(void) {
    tNodeIndex nodesToTx[NUM_TX_NODES_SCHEDULED];
    tSchedulerState scheduler = {0};
    uint8_t nodeTTL[MAX_NODES] = {0};
    // Create 5 nodes - First node is master
    for (uint32_t i=1; i<6; i++) {
        nodeTTL[i] = 0xFF;
    }
    // Test that the oldest is prioritised
    for (uint32_t i=1; i<6; i++) {
        scheduler.slotsSinceLastScheduled[i] = i+5;
    }
    for (uint32_t j=0; j<100; j++) {
        for (uint32_t i=1; i<6; i++) {
            schedulerUpdateAndCalcNextTxNodes(&scheduler, nodeTTL, MAX_NODES, nodesToTx);
            // The order should now be 5, 4, 3, 2, 1
            TEST_ASSERT_EQUAL_UINT(6-i, nodesToTx[0]);
        }
    }
}

void test_scheduler_with_different_buffer_levels_1(void) {
    tNodeIndex nodesToTx[NUM_TX_NODES_SCHEDULED];
    tSchedulerState scheduler = {0};
    uint8_t nodeTTL[MAX_NODES] = {0};
    // Create 5 nodes - First node is master
    for (uint32_t i=1; i<6; i++) {
        nodeTTL[i] = 0xFF;
    }
    // Test that the oldest is prioritised
    for (uint32_t i=1; i<6; i++) {
        scheduler.nodeTxBufferLevel[i] = i;
    }
    // Buffer levels are prioritised
    // We expect the order to be:
    uint8_t exp_nodes[20] = {
        5,
        4, 5,
        3, 4, 5,
        2, 3, 4, 5,
        1, 2, 3, 4, 5,
        1, 2, 3, 4, 5,
    };
    
    for (uint32_t j=0; j<20; j++) {
        schedulerUpdateAndCalcNextTxNodes(&scheduler, nodeTTL, MAX_NODES, nodesToTx);
        //printf("J:%d, expected:%d, got:%d\n", j, exp_nodes[j], nodesToTx[0]);
        TEST_ASSERT_EQUAL_UINT(exp_nodes[j], nodesToTx[0]);
    }
}
void test_scheduler_with_different_buffer_levels_2(void) {
    tNodeIndex nodesToTx[NUM_TX_NODES_SCHEDULED];
    tSchedulerState scheduler = {0};
    uint8_t nodeTTL[MAX_NODES] = {0};
    // Create 5 nodes - First node is master
    for (uint32_t i=1; i<6; i++) {
        nodeTTL[i] = 0xFF;
    }
    scheduler.nodeTxBufferLevel[5] = 5;
    scheduler.nodeTxBufferLevel[1] = 1;
    
    uint8_t exp_nodes[11] = {
        5, 5, 5, 5, 1, 5, 2, 3, 4, 1, 5
    };
    
    for (uint32_t j=0; j<11; j++) {
        schedulerUpdateAndCalcNextTxNodes(&scheduler, nodeTTL, MAX_NODES, nodesToTx);
        // printf("J:%d, expected:%d, got:%d\n", j, exp_nodes[j], nodesToTx[0]);
        TEST_ASSERT_EQUAL_UINT(exp_nodes[j], nodesToTx[0]);
    }
}


void test_scheduler_with_different_buffer_levels_and_max_latency(void) {
    tNodeIndex nodesToTx[NUM_TX_NODES_SCHEDULED];
    tSchedulerState scheduler = {0};
    uint8_t nodeTTL[MAX_NODES] = {0};
    // Create 2 nodes - First node is master
    nodeTTL[1] = 0xFF;
    nodeTTL[2] = 0xFF;
    scheduler.nodeTxBufferLevel[1] = 10;
    scheduler.nodeTxBufferLevel[2] = 0;
    scheduler.slotsSinceLastScheduled[2] = MAX_SLOTS_PER_NODE_TX_CHANCE;
    
    uint8_t exp_nodes[5] = {
        2, 1, 1, 1, 1, 
    };
    
    for (uint32_t j=0; j<5; j++) {
        schedulerUpdateAndCalcNextTxNodes(&scheduler, nodeTTL, MAX_NODES, nodesToTx);
        //printf("J:%d, expected:%d, got:%d\n", j, exp_nodes[j], nodesToTx[0]);
        TEST_ASSERT_EQUAL_UINT(exp_nodes[j], nodesToTx[0]);
    }
}


