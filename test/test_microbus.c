// TODO: what if the master transfer doesn't get enough bytes and therefore doesn't complete

#include "unity.h"
#include "simulator.h"
#include "microbus.h"
//#include "scheduler.h"
#include "packetChecker.h"

tSimulation sim = {0};

void halSetPs(tMasterNode * master, bool val) {
    setPs(&sim, master, val);
}
void halStartMasterTxRxDMA(tMasterNode * master, uint8_t * txData, uint8_t * rxData, uint32_t numBytes) {
    startMasterTxRxDMA(&sim, master, txData, rxData, numBytes);
}
void halStartSlaveTxRxDMA(tNode * node, bool tx, uint8_t * txData, uint8_t * rxData, uint32_t numBytes) {
    startSlaveTxRxDMA(&sim, node, tx, txData, rxData, numBytes);
}
void halStopNodeTxRxDMA(tNode * node) {
    stopTxRxDMA(&sim, node);
}

void test_simple(void) {
    #define NUM_TEST_NODES 2
    #define NUM_PACKETS 4
    
    //printf("Starting\n");
    simInit(&sim);
    
    tMasterNode * master = 0;
    tNode * nodes[NUM_TEST_NODES] = {};
    
    master = simCreateMaster(&sim, 2);
    master->scheduler.numSlots = NUM_TEST_NODES;
    
    // Create nodes
    for (uint32_t i=0; i<NUM_TEST_NODES; i++) {
        nodes[i] = simCreateSlave(&sim, i+1000);
        nodes[i]->nodeId = i;
        nodes[i]->txSlot = i;
        nodes[i]->scheduler.numSlots = NUM_TEST_NODES;
    }

    //// Simulate for long enough that all nodes have been assigned an ID
    //simulate(&sim, 20*1000*1000, true); // 20 millisecond
    
    // Create packets from master to all other nodes
    for (uint32_t dstNodeSimId=0; dstNodeSimId<NUM_TEST_NODES; dstNodeSimId++) {
        for (uint32_t j=0; j<NUM_PACKETS; j++) {
            simMasterCreateTxPacket(&sim, dstNodeSimId);
        }
    }
    
    // Create packets from all nodes to master
    for (uint32_t srcNodeSimId=0; srcNodeSimId<NUM_TEST_NODES; srcNodeSimId++) {
        for (uint32_t j=0; j<NUM_PACKETS; j++) {
            // Create packets to master
            simSlaveCreateTxPacket(&sim, srcNodeSimId);
        }
    }
    
    simulate(&sim, 1000*1000, true); // 1 second
    simCheckAllPacketsReceived(&sim);
    printf("Finished\n");
}


// void test_node_assignment(void) {
//     #define NUM_TEST_NODES 20
//     #define NUM_PACKETS 4
    
//     //printf("Starting\n");
//     simInit(&sim);
    
//     tMasterNode * master = 0;
//     tNode * nodes[NUM_TEST_NODES] = {};
    
//     master = simCreateMaster(&sim, 2);
    
//     // Create nodes
//     for (uint32_t i=0; i<NUM_TEST_NODES; i++) {
//         nodes[i] = simCreateSlave(&sim, i+2);
//         //nodes[i]->nodeId = i+1;
//         //nodes[i]->txSlot = i;
//         //nodes[i]->scheduler.numSlots = NUM_TEST_NODES;
//     }
    
//     // Create packets from master to all other nodes
//     for (uint32_t dstNodeSimId=0; dstNodeSimId<NUM_TEST_NODES; dstNodeSimId++) {
//         for (uint32_t j=0; j<NUM_PACKETS; j++) {
//             simCreatePacket(&sim, MASTER_NODE_SIM_ID, dstNodeSimId);
//         }
//     }
    
//     // Create packets from all nodes to master
//     for (uint32_t srcNodeSimId=0; srcNodeSimId<NUM_TEST_NODES; srcNodeSimId++) {
//         for (uint32_t j=0; j<NUM_PACKETS; j++) {
//             // Create packets to master
//             simCreatePacket(&sim, srcNodeSimId, MASTER_NODE_SIM_ID);
//         }
//     }
    
//     simulate(&sim, 1000*1000*1000); // 1 second
//     simCheckAllPacketsReceived(&sim);
//     printf("Finished\n");
// }