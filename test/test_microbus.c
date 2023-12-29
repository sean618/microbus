// TODO: what if the master transfer doesn't get enough bytes and therefore doesn't complete

#include "unity.h"
#include "simulator.h"
#include "microbus.h"

void test_simple(void) {
    printf("Starting\n");
    
    tMaster master = {0};
    tSlave slaves[2] = {0};
    uint32_t numSlaves = 2;
    
    tPacket mToSPkt[2] = {0};
    mToSPkt[0].data[0] = 1;
    mToSPkt[1].data[0] = 2;
    addTxPacket(&master.common, &mToSPkt[0]);
    addTxPacket(&master.common, &mToSPkt[1]);
    
    tPacket s0ToMPkt[2] = {0};
    s0ToMPkt[0].data[0] = 3;
    s0ToMPkt[1].data[0] = 4;
    addTxPacket(&slaves[0].common, &s0ToMPkt[0]);
    addTxPacket(&slaves[0].common, &s0ToMPkt[1]);
    
    tPacket s1ToMPkt[2] = {0};
    s1ToMPkt[0].data[0] = 5;
    s1ToMPkt[1].data[0] = 6;
    addTxPacket(&slaves[1].common, &s1ToMPkt[0]);
    addTxPacket(&slaves[1].common, &s1ToMPkt[1]);
    
    slaves[0].txSlot = 1;
    slaves[1].txSlot = 2;
    slaves[0].common.numSlots = 3;
    slaves[1].common.numSlots = 3;
    master.common.numSlots = 3;
    
    tSimulation * sim = simulate(&master, slaves, numSlaves, 1000*1000); // 1ms
    
    
    // Check master received slave packets
    TEST_ASSERT_EQUAL_UINT(4, master.common.rxPacketsReceived);
    TEST_ASSERT_EQUAL_UINT(s0ToMPkt[0].data[0], master.common.rxPacket[0].data[0]);
    TEST_ASSERT_EQUAL_UINT(s1ToMPkt[0].data[0], master.common.rxPacket[1].data[0]);
    TEST_ASSERT_EQUAL_UINT(s0ToMPkt[1].data[0], master.common.rxPacket[2].data[0]);
    TEST_ASSERT_EQUAL_UINT(s1ToMPkt[1].data[0], master.common.rxPacket[3].data[0]);
    
    // Check slaves received master packets
    TEST_ASSERT_EQUAL_UINT(2, slaves[0].common.rxPacketsReceived);
    TEST_ASSERT_EQUAL_UINT(mToSPkt[0].data[0], slaves[0].common.rxPacket[0].data[0]);
    TEST_ASSERT_EQUAL_UINT(mToSPkt[1].data[0], slaves[0].common.rxPacket[1].data[0]);
    
    TEST_ASSERT_EQUAL_UINT(2, slaves[1].common.rxPacketsReceived);
    TEST_ASSERT_EQUAL_UINT(mToSPkt[0].data[0], slaves[1].common.rxPacket[0].data[0]);
    TEST_ASSERT_EQUAL_UINT(mToSPkt[1].data[0], slaves[1].common.rxPacket[1].data[0]);
}