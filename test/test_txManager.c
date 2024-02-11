#include "stdlib.h"
#include "string.h"
#include "stdbool.h"
#include "stdint.h"
#include "unity.h"
#include "masterTxManager.h"
#include "usefulLib.h"


uint32_t myRand(uint32_t max) {
    if (max == 0)
        return 0;
    return (rand() % max); 
}

void test_simple(void) {
    // Simple - perfect transmission to 1 node
    tMasterTxManager manager = {0};
    uint8_t dstNodeId = 0;
    for (uint32_t i=0; i<50; i++) {
        masterAllocateTxPacket(&manager, dstNodeId);
    }
    for (uint32_t i=0; i<50; i++) {
        tPacket * packet = masterGetNextTxPacket(&manager);
        myAssert(packet != NULL, "");
        masterRxAckSeqNum(&manager, dstNodeId, packet->txSeqNum);
    }
    TEST_ASSERT_EQUAL_UINT(manager.packetStore.numStored, manager.packetStore.numFreed);
}

void test_continuous(void) {
    // Simple - perfect transmission to 1 node
    tMasterTxManager manager = {};
    uint8_t dstNodeId = 0;
    for (uint32_t i=0; i<50; i++) {
        masterAllocateTxPacket(&manager, dstNodeId);
        tPacket * packet = masterGetNextTxPacket(&manager);
        myAssert(packet != NULL, "");
        masterRxAckSeqNum(&manager, dstNodeId, packet->txSeqNum);
    }
    TEST_ASSERT_EQUAL_UINT(manager.packetStore.numStored, manager.packetStore.numFreed);
}

void test_multiple_nodes(void) {
    // Simple - perfect transmission to 1 node
    tMasterTxManager manager = {};
    for (uint32_t i=0; i<50; i++) {
        uint8_t dstNodeId = i % 5;
        masterAllocateTxPacket(&manager, dstNodeId);
    }
    for (uint32_t i=0; i<50; i++) {
        tPacket * packet = masterGetNextTxPacket(&manager);
        myAssert(packet != NULL, "");
        masterRxAckSeqNum(&manager, packet->nodeId, packet->txSeqNum);
    }
    TEST_ASSERT_EQUAL_UINT(manager.packetStore.numStored, manager.packetStore.numFreed);
}

void test_simple_with_dropped_packets(void) {
    tMasterTxManager manager = {};
    uint8_t dstNodeId = 0;
    for (uint32_t i=0; i<100; i++) {
        masterAllocateTxPacket(&manager, dstNodeId);
    }

    uint8_t lastAckSeqNum = 0;
    uint32_t drops = 0;
    for (uint32_t i=0; i<400; i++) {
        tPacket * packet = masterGetNextTxPacket(&manager);
        if (packet != NULL) {
            if (myRand(10) == 0) {
                drops++;
                // Skip tx
                //printf("Skipping\n");
            } else {
                //printf("Got:%u\n", packet->txSeqNum);
                if (packet->txSeqNum == lastAckSeqNum + 1) {
                    masterRxAckSeqNum(&manager, dstNodeId, packet->txSeqNum);
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
            masterRxAckSeqNum(&manager, dstNodeId, lastAckSeqNum);
        }
    }
    TEST_ASSERT_EQUAL_UINT(manager.packetStore.numStored, manager.packetStore.numFreed);
}


void test_multiple_nodes_with_dropped_packets(void) {
    for (uint32_t j=0; j<20; j++) {
        uint8_t lastAckSeqNum[5] = {0};
        tMasterTxManager manager = {0};

        for (uint32_t i=0; i<20; i++) {
            uint8_t dstNodeId = myRand(5);
            masterAllocateTxPacket(&manager, dstNodeId);
        }
        // Mode 0 - add packets as we go
        // Mode 1 - don't add anymore packets - just process them
        tNodeIndex rxNodeId = 0;
        for (uint8_t mode=0; mode<2; mode++) {
            for (uint32_t i=0; i<1000; i++) {
                if (mode == 0 && myRand(10) < 8) {
                    uint8_t dstNodeId = myRand(5);
                    tPacket * packet = masterAllocateTxPacket(&manager, dstNodeId);
                }
                
                tPacket * packet = masterGetNextTxPacket(&manager);

                if (packet != NULL) {
                    if (myRand(10) == 0) {
                        // Skip tx
                    } else {
                        if (packet->txSeqNum == lastAckSeqNum[packet->nodeId] + 1) {
                            lastAckSeqNum[packet->nodeId] = packet->txSeqNum;
                        }
                    }
                }

                rxNodeId++;
                if (rxNodeId == 5) {
                    rxNodeId = 0;
                }
                masterRxAckSeqNum(&manager, rxNodeId, lastAckSeqNum[rxNodeId]);
                
                if (mode == 1 && manager.packetStore.numStored == manager.packetStore.numFreed) {
                    //printf("Mode:%u - Cycles taken:%u\n", mode, i);
                    break;
                }
            }
        }
        TEST_ASSERT_EQUAL_UINT(manager.packetStore.numStored, manager.packetStore.numFreed);
    }
}