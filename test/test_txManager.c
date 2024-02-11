#include "stdlib.h"
#include "string.h"
#include "stdbool.h"
#include "stdint.h"
#include "unity.h"
#include "txManager.h"
#include "usefulLib.h"


uint32_t myRand(uint32_t max) {
    if (max == 0)
        return 0;
    return (rand() % max); 
}

static tTxManager manager;
static uint8_t txSeqNumStart[10];
static uint8_t txSeqNumEnd[10];
static uint8_t txSeqNumNext[10];
static uint8_t rxSeqNum[10];
static tTxPacketEntry packetEntries[100];

void test_simple(void) {
    // Simple - perfect transmission to 1 node
    initTxManager(&manager, 10, txSeqNumStart, txSeqNumEnd, txSeqNumNext, rxSeqNum, 100, packetEntries);
    
    uint8_t dstNodeId = 0;
    for (uint32_t i=0; i<100/2; i++) {
        allocateTxPacket(&manager, 0, dstNodeId);
    }
    for (uint32_t i=0; i<100/2; i++) {
        tPacket * packet = getNextTxPacket(&manager);
        myAssert(packet != NULL, "");
        rxAckSeqNum(&manager, dstNodeId, packet->txSeqNum);
    }
    TEST_ASSERT_EQUAL_UINT(manager.packetStore.numStored, manager.packetStore.numFreed);
}

void test_continuous(void) {
    // Simple - perfect transmission to 1 node
    initTxManager(&manager, 10, txSeqNumStart, txSeqNumEnd, txSeqNumNext, rxSeqNum, 100, packetEntries);
    uint8_t dstNodeId = 0;
    for (uint32_t i=0; i<100/2; i++) {
        allocateTxPacket(&manager, 0, dstNodeId);
        tPacket * packet = getNextTxPacket(&manager);
        myAssert(packet != NULL, "");
        rxAckSeqNum(&manager, dstNodeId, packet->txSeqNum);
    }
    TEST_ASSERT_EQUAL_UINT(manager.packetStore.numStored, manager.packetStore.numFreed);
}

void test_multiple_nodes(void) {
    // Simple - perfect transmission to 1 node
    initTxManager(&manager, 10, txSeqNumStart, txSeqNumEnd, txSeqNumNext, rxSeqNum, 100, packetEntries);
    for (uint32_t i=0; i<100/2; i++) {
        uint8_t dstNodeId = i % 5;
        allocateTxPacket(&manager, 0, dstNodeId);
    }
    for (uint32_t i=0; i<100/2; i++) {
        tPacket * packet = getNextTxPacket(&manager);
        myAssert(packet != NULL, "");
        rxAckSeqNum(&manager, packet->dstNodeId, packet->txSeqNum);
    }
    TEST_ASSERT_EQUAL_UINT(manager.packetStore.numStored, manager.packetStore.numFreed);
}

void test_simple_with_dropped_packets(void) {
    initTxManager(&manager, 10, txSeqNumStart, txSeqNumEnd, txSeqNumNext, rxSeqNum, 100, packetEntries);
        
    uint8_t dstNodeId = 0;
    for (uint32_t i=0; i<100/2; i++) {
        allocateTxPacket(&manager, 0, dstNodeId);
    }

    uint8_t lastAckSeqNum = 0;
    uint32_t drops = 0;
    for (uint32_t i=0; i<4*100; i++) {
        tPacket * packet = getNextTxPacket(&manager);
        if (packet != NULL) {
            if (myRand(10) == 0) {
                drops++;
                // Skip tx
                //printf("Skipping\n");
            } else {
                //printf("Got:%u\n", packet->txSeqNum);
                if (packet->txSeqNum == lastAckSeqNum + 1) {
                    rxAckSeqNum(&manager, dstNodeId, packet->txSeqNum);
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
            rxAckSeqNum(&manager, dstNodeId, lastAckSeqNum);
        }
    }
    TEST_ASSERT_EQUAL_UINT(manager.packetStore.numStored, manager.packetStore.numFreed);
}


void test_multiple_nodes_with_dropped_packets(void) {
    for (uint32_t j=0; j<20; j++) {
        uint8_t lastAckSeqNum[5] = {0};
        initTxManager(&manager, 10, txSeqNumStart, txSeqNumEnd, txSeqNumNext, rxSeqNum, 100, packetEntries);

        for (uint32_t i=0; i<20; i++) {
            uint8_t dstNodeId = myRand(5);
            allocateTxPacket(&manager, 0, dstNodeId);
        }
        // Mode 0 - add packets as we go
        // Mode 1 - don't add anymore packets - just process them
        tNodeIndex rxNodeId = 0;
        for (uint8_t mode=0; mode<2; mode++) {
            for (uint32_t i=0; i<1000; i++) {
                if (mode == 0 && myRand(10) < 8) {
                    uint8_t dstNodeId = myRand(5);
                    tPacket * packet = allocateTxPacket(&manager, 0, dstNodeId);
                }
                
                tPacket * packet = getNextTxPacket(&manager);

                if (packet != NULL) {
                    if (myRand(10) == 0) {
                        // Skip tx
                    } else {
                        if (packet->txSeqNum == lastAckSeqNum[packet->dstNodeId] + 1) {
                            lastAckSeqNum[packet->dstNodeId] = packet->txSeqNum;
                        }
                    }
                }

                rxNodeId++;
                if (rxNodeId == 5) {
                    rxNodeId = 0;
                }
                rxAckSeqNum(&manager, rxNodeId, lastAckSeqNum[rxNodeId]);
                
                if (mode == 1 && manager.packetStore.numStored == manager.packetStore.numFreed) {
                    //printf("Mode:%u - Cycles taken:%u\n", mode, i);
                    break;
                }
            }
        }
        TEST_ASSERT_EQUAL_UINT(manager.packetStore.numStored, manager.packetStore.numFreed);
    }
}