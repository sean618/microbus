// TODO: what if the master transfer doesn't get enough bytes and therefore doesn't complete

#include "stdlib.h"
#include "unity.h"
#include "simulator.h"
#include "microbus.h"
#include "scheduler.h"
#include "packetChecker.h"

//tSimulation sim = {0};

void halSetPs(tNode * master, bool val) {
    //setPs(&sim, master, val);
}
void halStartNodeTxRxDMA(tNode * node, bool tx, uint8_t * txData, uint8_t * rxData, uint32_t numBytes) {
    //startTxRxDMA(&sim, node, tx, txData, rxData, numBytes);
}
void halStopNodeTxRxDMA(tNode * node) {
    //stopTxRxDMA(&sim, node);
}

// Not ideal - more weighting to lower values!
uint32_t myRand(uint32_t max) {
    return (rand() % max); 
}

void checkSlaveScheduleMatches(tMasterSchedulerState * mScheduler, tSlaveSchedulerState * sScheduler, 
                                uint8_t slaveNumSlotEntries, uint32_t slaveTotalNumSlots) {
    
    TEST_ASSERT_EQUAL_UINT(true, sScheduler->schedulerInitialised);
    
    TEST_ASSERT_EQUAL_UINT(0, sScheduler->expSchedulerPacketIndex);
    TEST_ASSERT_EQUAL_UINT(mScheduler->totalSchedulerPackets, sScheduler->totalSchedulerPackets);
    TEST_ASSERT_EQUAL_UINT(mScheduler->newNodeId, sScheduler->newNodeId);
    TEST_ASSERT_EQUAL_UINT(mScheduler->newNodeUniqueId, sScheduler->newNodeUniqueId);
    TEST_ASSERT_EQUAL_UINT(mScheduler->schedulerPeriodInFrames, sScheduler->schedulerPeriodInFrames);
    TEST_ASSERT_EQUAL_UINT(mScheduler->numSlotsPerFrame, sScheduler->numSlotsPerFrame);
    TEST_ASSERT_EQUAL_UINT(slaveNumSlotEntries, sScheduler->numMySlotEntries);
    uint32_t totalSlots = 0;
    for (uint8_t i=0; i<sScheduler->numMySlotEntries; i++) {
        totalSlots += sScheduler->mySlotLengths[i];
    }
    TEST_ASSERT_EQUAL_UINT(slaveTotalNumSlots, totalSlots);
}

void test_scheduler_packing_and_unpacking(void) {
    printf("Starting\n");
    tMasterSchedulerState mScheduler = {0};
    tSlaveSchedulerState sScheduler = {0};
    
    for (uint8_t i=0; i<20; i++) {
        
        uint8_t slaveNodeId = myRand(MAX_NODES);
        mScheduler.newNodeId = rand();
        mScheduler.newNodeUniqueId = ((uint64_t)rand() << 32) | rand();
        mScheduler.schedulerPeriodInFrames = rand();
        mScheduler.numSlotsPerFrame = rand();
        
        mScheduler.numSlotEntries = myRand(MAX_SLOT_ENTRIES);
        uint8_t slaveNumSlotEntries = 0;
        uint32_t slaveTotalNumSlots = 0;
        
        for (uint16_t j=0; j<mScheduler.numSlotEntries; j++) {
            uint8_t nodeId;
            uint8_t numSlots = myRand(100);
            
            if (myRand(5) == 0 && slaveNumSlotEntries < MAX_NODE_SLOT_ENTRIES) {
                nodeId = slaveNodeId;
                slaveNumSlotEntries++;
                slaveTotalNumSlots += numSlots;
            } else {
                do {
                    nodeId = rand();
                } while (nodeId == slaveNodeId);
            }
            
            mScheduler.slotEntry[j].nodeId = nodeId;
            mScheduler.slotEntry[j].numSlots = numSlots;
        }
        
        do {
            tPacket packet = {0};
            packNextSchedulerPacket(&mScheduler, &packet);
            unpackSchedulerPacket(slaveNodeId, &sScheduler, &packet);
        } while (mScheduler.schedulerPacketIndex != 0);
        
        checkSlaveScheduleMatches(&mScheduler, &sScheduler, slaveNumSlotEntries, slaveTotalNumSlots);
    }
    printf("Finished\n");
}