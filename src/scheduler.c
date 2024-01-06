#include "string.h"
#include "stdbool.h"
#include "stdint.h"
#include "stdio.h"

//#include "cexception.h"

#include "microbus.h"
#include "scheduler.h"

// void updateScheduler(tMasterSchedulerState * scheduler) {
    
//     scheduler->numEntries
    
//     scheduler->protocolVersion = MICROBUS_VERSION;
//     scheduler->
// }

void packNextSchedulerPacket(tMasterSchedulerState * scheduler, tPacket * packet) {
    // At the start work out how many scheduler packets are needed
    if (scheduler->schedulerPacketIndex == 0) {
        int32_t entriesRemaining = scheduler->numSlotEntries;
        uint8_t totalSchedulerPackets = 0;
        while (entriesRemaining > 0) {
            entriesRemaining -= ENTRIES_PER_PACKET;
            totalSchedulerPackets++;
        }
        scheduler->totalSchedulerPackets = totalSchedulerPackets;
    }
    
    packet->data[0] = MICROBUS_VERSION;
    packet->data[1] = scheduler->schedulerPacketIndex;
    packet->data[2] = scheduler->totalSchedulerPackets;
    packet->data[3] = scheduler->newNodeId;
    memcpy(&packet->data[4], &scheduler->newNodeUniqueId, 8);
    packet->data[12] = scheduler->schedulerPeriodInFrames; // A scheduler is sent every this many frames
    packet->data[13] = scheduler->numSlotsPerFrame >> 8;
    packet->data[14] = scheduler->numSlotsPerFrame & 0xFF;
    
    uint32_t entryIndex = ENTRIES_PER_PACKET * scheduler->schedulerPacketIndex;
    uint32_t entriesRemaining = scheduler->numSlotEntries - entryIndex;
    uint8_t numSlotEntries = MIN(entriesRemaining, ENTRIES_PER_PACKET);
    packet->data[15] = numSlotEntries;
    for (uint8_t i=0; i<numSlotEntries; i++) {
        packet->data[16 + 2*i] = scheduler->slotEntry[entryIndex + i].nodeId;
        packet->data[17 + 2*i] = scheduler->slotEntry[entryIndex + i].numSlots;
    }
    scheduler->schedulerPacketIndex++;
    if (scheduler->schedulerPacketIndex == scheduler->totalSchedulerPackets) {
        scheduler->schedulerPacketIndex = 0;
    }
}

void unpackSchedulerPacket(uint8_t nodeId, tSlaveSchedulerState * scheduler, tPacket * packet) {
    uint8_t protocolVersion = packet->data[0];
    uint8_t schedulerPacketIndex = packet->data[1];
    
    myAssert(packet->data[0] == MICROBUS_VERSION, "Invalid protocol version");
    if (packet->data[0] != MICROBUS_VERSION) {
        return;
    }
    if (schedulerPacketIndex != scheduler->expSchedulerPacketIndex) {
        // If a packet is out of order - Reset ready for next time!
        scheduler->expSchedulerPacketIndex = 0;
        scheduler->schedulerInitialised = false;
        return;
    }
    if (schedulerPacketIndex == 0) {
        scheduler->schedulerInitialised = false;
        scheduler->numMySlotEntries = 0;
    }

    scheduler->expSchedulerPacketIndex = schedulerPacketIndex;
    scheduler->totalSchedulerPackets = packet->data[2];
    scheduler->newNodeId = packet->data[3];
    memcpy(&scheduler->newNodeUniqueId, &packet->data[4], 8);
    scheduler->schedulerPeriodInFrames = packet->data[12];
    scheduler->numSlotsPerFrame = (uint16_t) packet->data[13] << 8 | packet->data[14];
    uint8_t numSlotEntries = packet->data[15];
    
    // Convert master slot description to slave slot description
    tSlot slotIndex = 0;
    for (uint8_t i=0; i<numSlotEntries; i++) {
        uint8_t rxNodeId = packet->data[16 + 2*i];
        uint8_t numSlots = packet->data[17 + 2*i];
        if (rxNodeId == nodeId) {
            scheduler->myStartSlots[scheduler->numMySlotEntries] = slotIndex;
            scheduler->mySlotLengths[scheduler->numMySlotEntries] = numSlots;
            scheduler->numMySlotEntries++;
        }
        slotIndex += numSlots;
    }
    
    // Check if we've reached the end of the scheduler packets
    scheduler->expSchedulerPacketIndex++;
    if (scheduler->expSchedulerPacketIndex == scheduler->totalSchedulerPackets) {
        scheduler->expSchedulerPacketIndex = 0;
        scheduler->schedulerInitialised = true;
    }
}




