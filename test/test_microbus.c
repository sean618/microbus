// TODO: what if the master transfer doesn't get enough bytes and therefore doesn't complete

#include "unity.h"
#include "simulator.h"
#include "microbus.h"

void test_simple(void) {
    printf("Starting\n");
    
    tMaster master = {0};
    tNode nodes[2] = {0};
    uint32_t numNodes = 2;
    
    tPacket mToSPkt[4] = {0};
    mToSPkt[0].data[0] = 91;
    mToSPkt[1].data[0] = 92;
    mToSPkt[2].data[0] = 93;
    mToSPkt[3].data[0] = 94;
    addTxPacket(&master.common, &mToSPkt[0]);
    addTxPacket(&master.common, &mToSPkt[1]);
    addTxPacket(&master.common, &mToSPkt[2]);
    addTxPacket(&master.common, &mToSPkt[3]);
    
    tPacket s0ToMPkt[2] = {0};
    s0ToMPkt[0].data[0] = 1;
    s0ToMPkt[1].data[0] = 2;
    addTxPacket(&nodes[0].common, &s0ToMPkt[0]);
    addTxPacket(&nodes[0].common, &s0ToMPkt[1]);
    
    tPacket s1ToMPkt[2] = {0};
    s1ToMPkt[0].data[0] = 11;
    s1ToMPkt[1].data[0] = 12;
    addTxPacket(&nodes[1].common, &s1ToMPkt[0]);
    addTxPacket(&nodes[1].common, &s1ToMPkt[1]);
    
    nodes[0].txSlot = 0;
    nodes[1].txSlot = 1;
    nodes[0].common.numSlots = 2;
    nodes[1].common.numSlots = 2;
    master.common.numSlots = 2;
    
    tSimulation * sim = simulate(&master, nodes, numNodes, 1000*1000); // 1ms
    
    
    // Check master received node packets
    TEST_ASSERT_EQUAL_UINT(4, master.common.rxPacketsReceived);
    TEST_ASSERT_EQUAL_UINT(s0ToMPkt[0].data[0], master.common.rxPacket[0].data[0]);
    TEST_ASSERT_EQUAL_UINT(s1ToMPkt[0].data[0], master.common.rxPacket[1].data[0]);
    TEST_ASSERT_EQUAL_UINT(s0ToMPkt[1].data[0], master.common.rxPacket[2].data[0]);
    TEST_ASSERT_EQUAL_UINT(s1ToMPkt[1].data[0], master.common.rxPacket[3].data[0]);
    
    // Check nodes received master packets
    TEST_ASSERT_EQUAL_UINT(4, nodes[0].common.rxPacketsReceived);
    TEST_ASSERT_EQUAL_UINT(mToSPkt[0].data[0], nodes[0].common.rxPacket[0].data[0]);
    TEST_ASSERT_EQUAL_UINT(mToSPkt[1].data[0], nodes[0].common.rxPacket[1].data[0]);
    TEST_ASSERT_EQUAL_UINT(mToSPkt[2].data[0], nodes[0].common.rxPacket[2].data[0]);
    TEST_ASSERT_EQUAL_UINT(mToSPkt[3].data[0], nodes[0].common.rxPacket[3].data[0]);
    
    TEST_ASSERT_EQUAL_UINT(4, nodes[1].common.rxPacketsReceived);
    TEST_ASSERT_EQUAL_UINT(mToSPkt[0].data[0], nodes[1].common.rxPacket[0].data[0]);
    TEST_ASSERT_EQUAL_UINT(mToSPkt[1].data[0], nodes[1].common.rxPacket[1].data[0]);
    TEST_ASSERT_EQUAL_UINT(mToSPkt[2].data[0], nodes[1].common.rxPacket[2].data[0]);
    TEST_ASSERT_EQUAL_UINT(mToSPkt[3].data[0], nodes[1].common.rxPacket[3].data[0]);
}