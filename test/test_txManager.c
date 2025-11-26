// Copyright (c) 2025 Sean Bremner
// Licensed under the MIT License. See LICENSE file for details.


#include "stdlib.h"
#include "string.h"
#include "stdbool.h"
#include "stdint.h"
#include "assert.h"

#include "../src/microbus.h"
#include "../src/txManager.h"
#include "../src/node.h"
#include "../src/networkManager.h"



uint32_t myRand(uint32_t max) {
    if (max == 0)
        return 0;
    return (rand() % max); 
}

static tTxManager manager;
static uint8_t txSeqNumStart[10];
static uint8_t txSeqNumEnd[10];
static uint8_t txSeqNumNext[10];
static uint8_t txSeqNumPauseCount[10];
static uint8_t rxSeqNum[10];
static tPacketEntry packetEntries[100];
static tNodeIndex activeTxNodeIds[10];
static tNodeQueue activeTxNodes;
uint64_t txWindowRestarts;
uint8_t ttl = 1;

static void basicInit() {
    activeTxNodes.numNodes = 0;
    activeTxNodes.lastIndex = 0;
    initTxManager(&manager, 10, txSeqNumStart, txSeqNumEnd, txSeqNumNext, txSeqNumPauseCount, rxSeqNum, &activeTxNodes, 100, packetEntries);
}

static void createMasterTxPacket(tTxManager * txManager, tNodeIndex dstNodeId, bool allowFull) {
    tPacket * packet = allocateTxPacket(txManager, MASTER_NODE_ID);
    if (allowFull && packet == NULL) {
        return;
    }
    submitAllocatedTxPacket(txManager, true, &ttl, MASTER_NODE_ID, dstNodeId, MASTER_DATA_PACKET, 1);
}

void test_simple(void) {
    // Simple - perfect transmission to 1 node
    basicInit();
    
    // Create 50 tx packets on master to node
    uint8_t dstNodeId = 1;
    for (uint32_t i=0; i<100/2; i++) {
        createMasterTxPacket(&manager, dstNodeId, false);
    }
    // Check that we can retrieve those 50 packets from the tx manager
    for (uint32_t i=0; i<100/2; i++) {
        tNodeIndex nextTxNodeId = dstNodeId;
        tPacket * packet = masterGetNextTxDataPacket(&manager, 1, &nextTxNodeId, 1);
        assert(packet != NULL);
        rxAckSeqNum(&manager, dstNodeId, packet->txSeqNum, true, &txWindowRestarts);
    }
    assert(manager.packetStore.numStored == manager.packetStore.numFreed);
}

void test_continuous(void) {
    basicInit();
    uint8_t dstNodeId = 0;
    for (uint32_t i=0; i<100/2; i++) {
        createMasterTxPacket(&manager, dstNodeId, false);
        tNodeIndex nextTxNodeId = dstNodeId;
        tPacket * packet = masterGetNextTxDataPacket(&manager, 1, &nextTxNodeId, 1);
        assert(packet != NULL);
        rxAckSeqNum(&manager, dstNodeId, packet->txSeqNum, true, &txWindowRestarts);
    }
    assert(manager.packetStore.numStored == manager.packetStore.numFreed);
}

void test_multiple_nodes(void) {
    basicInit();
    for (uint32_t i=0; i<100/2; i++) {
        uint8_t dstNodeId = i % 5;
        createMasterTxPacket(&manager, dstNodeId, false);
    }
    for (uint32_t i=0; i<100/2; i++) {
        uint8_t dstNodeId = i % 5;
        tNodeIndex nextTxNodeId = dstNodeId;
        tPacket * packet = masterGetNextTxDataPacket(&manager, 1, &nextTxNodeId, 1);
        assert(packet != NULL);
        rxAckSeqNum(&manager, packet->master.dstNodeId, packet->txSeqNum, true, &txWindowRestarts);
    }
    assert(manager.packetStore.numStored == manager.packetStore.numFreed);
}

void test_simple_with_dropped_packets(void) {
    basicInit();
        
    uint8_t dstNodeId = 0;
    for (uint32_t i=0; i<100/2; i++) {
        createMasterTxPacket(&manager, dstNodeId, false);
    }

    uint8_t lastAckSeqNum = 0;
    uint32_t drops = 0;
    for (uint32_t i=0; i<4*100; i++) {
        tNodeIndex nextTxNodeId = dstNodeId;
        tPacket * packet = masterGetNextTxDataPacket(&manager, 1, &nextTxNodeId, 1);
        if (packet != NULL) {
            if (myRand(10) == 0) {
                drops++;
                // Skip tx
                //printf("Skipping\n");
            } else {
                //printf("Got:%u\n", packet->txSeqNum);
                if (packet->txSeqNum == lastAckSeqNum + 1) {
                    rxAckSeqNum(&manager, dstNodeId, packet->txSeqNum, true, &txWindowRestarts);
                    //printf("Acked:%u\n", packet->txSeqNum);
                    lastAckSeqNum = packet->txSeqNum;

                    if (manager.packetStore.numStored == manager.packetStore.numFreed) {
                        //printf("Cycles taken:%u for 100 packets with %u drops\n", i, drops);
                        break;
                    }
                }
            }
        } else {
            //printf("Acked:%u\n", lastAckSeqNum);
            rxAckSeqNum(&manager, dstNodeId, lastAckSeqNum, true, &txWindowRestarts);
        }
    }
    assert(manager.packetStore.numStored == manager.packetStore.numFreed);
}


void test_multiple_nodes_with_dropped_packets(void) {
    for (uint32_t j=0; j<20; j++) {
        uint8_t lastAckSeqNum[5] = {0};
        basicInit();

        for (uint32_t i=0; i<20; i++) {
            uint8_t dstNodeId = myRand(5);
            createMasterTxPacket(&manager, dstNodeId, false);
        }
        // Mode 0 - add packets as we go
        // Mode 1 - don't add anymore packets - just process them
        tNodeIndex rxNodeId = 0;
        for (uint8_t mode=0; mode<2; mode++) {
            for (uint32_t i=0; i<1000; i++) {
                if (mode == 0 && myRand(10) < 8) {
                    uint8_t dstNodeId = myRand(5);
                    createMasterTxPacket(&manager, dstNodeId, true);
                }
                
                uint8_t dstNodeId = myRand(5);
                tNodeIndex nextTxNodeId = dstNodeId;
                tPacket * packet = masterGetNextTxDataPacket(&manager, 1, &nextTxNodeId, 1);

                if (packet != NULL) {
                    if (myRand(10) == 0) {
                        // Skip tx
                    } else {
                        if (packet->txSeqNum == lastAckSeqNum[packet->master.dstNodeId] + 1) {
                            lastAckSeqNum[packet->master.dstNodeId] = packet->txSeqNum;
                        }
                    }
                }

                rxNodeId++;
                if (rxNodeId == 5) {
                    rxNodeId = 0;
                }
                rxAckSeqNum(&manager, rxNodeId, lastAckSeqNum[rxNodeId], true, &txWindowRestarts);
                
                if (mode == 1 && manager.packetStore.numStored == manager.packetStore.numFreed) {
                    //printf("Mode:%u - Cycles taken:%u\n", mode, i);
                    break;
                }
            }
        }
        assert(manager.packetStore.numStored == manager.packetStore.numFreed);
    }
}

void test_continuous_with_delayed_acks(void) {
    basicInit();
    uint8_t dstNodeId = 1;
    uint8_t lastTxSeqNum = 0;
    for (uint32_t i=0; i<100/2; i++) {
        createMasterTxPacket(&manager, dstNodeId, false);
        tNodeIndex nextTxNodeId = dstNodeId;
        tPacket * packet = masterGetNextTxDataPacket(&manager, 1, &nextTxNodeId, 1);
        assert(packet != NULL);
        lastTxSeqNum = MAX(packet->txSeqNum, lastTxSeqNum);

        // If we ack just before the sliding window restarts
        if (i % (SLIDING_WINDOW_SIZE - 1) == 1) {
            rxAckSeqNum(&manager, dstNodeId, lastTxSeqNum-1, true, &txWindowRestarts);
        }
    }
    assert(manager.packetStore.numStored == manager.packetStore.numFreed+1);
    assert(manager.packetStore.numStored == 50);
}

void test_continuous_with_extra_delayed_acks(void) {
    basicInit();
    uint8_t dstNodeId = 1;
    uint8_t lastTxSeqNum = 0;
    for (uint32_t i=0; i<100/2; i++) {
        createMasterTxPacket(&manager, dstNodeId, false);
        tNodeIndex nextTxNodeId = dstNodeId;
        tPacket * packet = masterGetNextTxDataPacket(&manager, 1, &nextTxNodeId, 1);
        if (packet) {
            lastTxSeqNum = MAX(packet->txSeqNum, lastTxSeqNum);
        }

        // If we ack just before the sliding window restarts
        if (i % (SLIDING_WINDOW_SIZE) == 1) {
            rxAckSeqNum(&manager, dstNodeId, lastTxSeqNum-1, true, &txWindowRestarts);
        }
    }
    // Should be 3/4 of the bandwidth as every 1 in 4 packets is delayed because the ack hasn't arrived
    assert(manager.packetStore.numFreed == 37);
    assert(manager.packetStore.numStored == 50);
}

void test_continuous_with_extra_delayed_acks_2(void) {
    basicInit();
    uint8_t dstNodeId = 1;
    uint8_t lastTxSeqNum = 0;
    for (uint32_t i=0; i<100/2; i++) {
        createMasterTxPacket(&manager, dstNodeId, false);
        tNodeIndex nextTxNodeId = dstNodeId;
        tPacket * packet = masterGetNextTxDataPacket(&manager, 1, &nextTxNodeId, 1);
        if (packet) {
            lastTxSeqNum = MAX(packet->txSeqNum, lastTxSeqNum);
        }

        // If we ack just before the sliding window restarts
        if (i % (SLIDING_WINDOW_SIZE + 2) == 1) {
            rxAckSeqNum(&manager, dstNodeId, lastTxSeqNum-1, true, &txWindowRestarts);
        }
    }
    // Should be 3/6 of the bandwidth
    assert(manager.packetStore.numFreed == 25);
    assert(manager.packetStore.numStored == 50);
}

void testTxManager() {
    test_simple();
    test_continuous();
    test_multiple_nodes();
    test_simple_with_dropped_packets();
    test_multiple_nodes_with_dropped_packets();
    test_continuous_with_delayed_acks();
    test_continuous_with_extra_delayed_acks();
    test_continuous_with_extra_delayed_acks_2();
}

