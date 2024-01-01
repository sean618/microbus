// TODO: what if the master transfer doesn't get enough bytes and therefore doesn't complete

#include "unity.h"
#include "simulator.h"
#include "microbus.h"
#include "packetChecker.h"

tSimulation sim = {0};

void halSetPs(tNode * master, bool val) {
    setPs(&sim, master, val);
}
void halStartNodeTxRxDMA(tNode * node, bool tx, uint8_t * txData, uint8_t * rxData, uint32_t numBytes) {
    startTxRxDMA(&sim, node, tx, txData, rxData, numBytes);
}
void halStopNodeTxRxDMA(tNode * node) {
    stopTxRxDMA(&sim, node);
}

void test_simple(void) {
    #define NUM_TEST_NODES 20
    #define NUM_PACKETS 4
    
    printf("Starting\n");
    simInit(&sim);
    
    tNode * nodes[NUM_TEST_NODES] = {};
    
    // Create nodes
    for (uint32_t i=0; i<NUM_TEST_NODES; i++) {
        nodes[i] = simCreateNode(&sim, (i == MASTER_NODE_ID));
        nodes[i]->nodeId = i;
        nodes[i]->txSlot = i;
        nodes[i]->numSlots = NUM_TEST_NODES;
    }
    
    // Create packets from master to all other nodes
    for (uint32_t dstNodeSimId=MASTER_NODE_ID+1; dstNodeSimId<NUM_TEST_NODES; dstNodeSimId++) {
        for (uint32_t j=0; j<NUM_PACKETS; j++) {
            simCreatePacket(&sim, MASTER_NODE_ID, dstNodeSimId);
        }
    }
    
    // Create packets from all nodes to master
    for (uint32_t srcNodeSimId=MASTER_NODE_ID+1; srcNodeSimId<NUM_TEST_NODES; srcNodeSimId++) {
        for (uint32_t j=0; j<NUM_PACKETS; j++) {
            // Create packets to master
            simCreatePacket(&sim, srcNodeSimId, MASTER_NODE_ID);
        }
    }
    
    simulate(&sim, 1000*1000*1000); // 1 second
    simCheckAllPacketsReceived(&sim);
}