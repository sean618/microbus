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
    uint32_t byteIndex = SCHEDULE_PACKET_FIXED_BYTES;
    
    // Fill in new node entries
    // while there are new nodes remaining and enough bytes left in the packet
    uint32_t numNewNodes = 0;
    while ((scheduler->numNewNodes > scheduler->newNodeIndex)
            && ((byteIndex + SIZE_OF_NEW_NODE_ENTRY) < PACKET_SIZE)) {
        packet->data[byteIndex] = scheduler->newNodeEntry[scheduler->newNodeIndex].newNodeId;
        memcpy(&packet->data[byteIndex+1], &scheduler->newNodeEntry[scheduler->newNodeIndex].newNodeUniqueId, 8);
        scheduler->newNodeIndex++;
        numNewNodes++;
        byteIndex += SIZE_OF_NEW_NODE_ENTRY;
    }
    
    // Fill in slot entries
    // while there are slots remaining and enough bytes left in the packet
    uint32_t numSlotEntries = 0;
    while ((scheduler->numSlotEntries > scheduler->slotEntryIndex)
            && ((byteIndex + SIZE_OF_SLOT_ENTRY) < PACKET_SIZE)) {
        packet->data[byteIndex  ] = scheduler->slotEntry[scheduler->slotEntryIndex].nodeId;
        packet->data[byteIndex+1] = scheduler->slotEntry[scheduler->slotEntryIndex].numSlots;
        scheduler->slotEntryIndex++;
        numSlotEntries++;
        byteIndex += SIZE_OF_SLOT_ENTRY;
    }
    
    bool finalSchedulerPacket = ((scheduler->numSlotEntries == scheduler->slotEntryIndex)
                                && (scheduler->numNewNodes == scheduler->newNodeIndex));
    
    packet->data[0] = MICROBUS_VERSION;
    packet->data[1] = scheduler->schedulerPacketIndex;
    packet->data[2] = finalSchedulerPacket;
    packet->data[3] = scheduler->schedulerPeriodInFrames; // A scheduler is sent every this many frames
    packet->data[4] = scheduler->numSlotsPerFrame >> 8;
    packet->data[5] = scheduler->numSlotsPerFrame & 0xFF;
    packet->data[6] = numSlotEntries;
    packet->data[7] = numNewNodes;
    
    // Increment packet index and reset state if reached the end
    scheduler->schedulerPacketIndex++;
    if (finalSchedulerPacket) {
        // TODO: use memset
        scheduler->schedulerPacketIndex = 0;
        scheduler->slotEntryIndex = 0;
        scheduler->newNodeIndex = 0;
    }
}

void unpackSchedulerPacket(tSlaveSchedulerState * scheduler, tPacket * packet, uint64_t unqiueNodeId, uint8_t * nodeId) {
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
    bool finalSchedulerPacket = packet->data[2];
    scheduler->schedulerPeriodInFrames = packet->data[3];
    scheduler->numSlotsPerFrame = (uint16_t) packet->data[4] << 8 | packet->data[5];
    uint8_t numSlotEntries = packet->data[6];
    uint8_t numNewNodes = packet->data[7];
    
    uint32_t byteIndex = 8;
    
    // Check if we are in the list of new node IDs
    for (uint8_t i=0; i<numNewNodes; i++) {
        uint64_t newNodeUniqueId;
        uint8_t newNodeId = packet->data[byteIndex];
        memcpy(&newNodeUniqueId, &packet->data[byteIndex+1], 8);
        byteIndex += SIZE_OF_NEW_NODE_ENTRY;
        
        if (newNodeUniqueId == unqiueNodeId) {
            *nodeId = newNodeId;
        }
    }
    
    // Convert master slot description to slave slot description
    tSlot slotIndex = scheduler->slotIndex;
    for (uint8_t i=0; i<numSlotEntries; i++) {
        uint8_t rxNodeId = packet->data[byteIndex];
        uint8_t numSlots = packet->data[byteIndex+1];
        byteIndex += SIZE_OF_SLOT_ENTRY;
        // Only record slots that are for us
        if (rxNodeId == *nodeId) {
            scheduler->myStartSlots[scheduler->numMySlotEntries] = slotIndex;
            scheduler->mySlotLengths[scheduler->numMySlotEntries] = numSlots;
            scheduler->numMySlotEntries++;
        }
        slotIndex += numSlots;
    }
    
    // Check if we've reached the end of the scheduler packets
    scheduler->expSchedulerPacketIndex++;
    if (finalSchedulerPacket) {
        // TODO: use memset
        scheduler->expSchedulerPacketIndex = 0;
        scheduler->slotIndex = 0;
        scheduler->schedulerInitialised = true;
    }
}




