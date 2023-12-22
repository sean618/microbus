#include "unity.h"
#include "microbus.h"


void simulate(tMaster * master, tSlave slave[], uint32_t numSlaves, uint64_t numSimulationSlots) {
    for (uint64_t i=0; i<numSimulationSlots; i++) {
        // Transmit master to slave packet
        tPacket * txPacket = getTxPacket(&master->common);
        if (txPacket) {
            for (uint32_t s=0; s<numSlaves; s++) {
                printf("Adding packet %d to slave %d\n", txPacket->val, s);
                addRxPacket(&slave[s].common, txPacket);
                removeTxPacket(&master->common);
            }
        }
        for (uint32_t s=0; s<numSlaves; s++) {
            // Transmit slave to master packet
            txPacket = getTxPacket(&slave[s].common);
            if (txPacket) {
                printf("Adding packet %d from slave %d to master\n", txPacket->val, s);
                addRxPacket(&master->common, txPacket);
                removeTxPacket(&slave[s].common);
            }
        }
    }
}

void test_simple(void) {
    tMaster master = {0};
    tSlave slave[2] = {0};
    uint32_t numSlaves = 2;
    
    tPacket packet[3] = {{1},{2},{3}};
    addTxPacket(&master.common, &packet[0]);
    addTxPacket(&slave[0].common, &packet[1]);
    addTxPacket(&slave[1].common, &packet[2]);
    
    simulate(&master, slave, numSlaves, 100);
    
    // Check master received slave packets
    TEST_ASSERT_EQUAL_UINT(packet[1].val, master.common.rxPacket[0].val);
    TEST_ASSERT_EQUAL_UINT(packet[2].val, master.common.rxPacket[1].val);
    
    // Check slaves received master packets
    TEST_ASSERT_EQUAL_UINT(packet[0].val, slave[0].common.rxPacket[0].val);
    TEST_ASSERT_EQUAL_UINT(packet[0].val, slave[1].common.rxPacket[0].val);
}