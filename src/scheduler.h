#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "stdint.h"
#include "microbus.h"

// Schedule packet;
// - Protocol version - 1 bytes
// - schedule packet index - 4 bits
// - total schedule packets - 4 bits
// - num frame slots - 2 bytes
// - New node Unique Id - 8 bytes
// - New node Id - tNodeId - 1 byte
// - num frames before next schedule - 1 byte
// - numPacketEntries - 2 bytes
//      - Node Id (1 byte)
//      - Allocated num slots (1 bytes)
//        repeated numPacketEntries times

// NOTE: only 1 byte when total slots is 1024
// if more than 255 slots needed use multiple entries
typedef struct {
    uint8_t nodeId;
    uint8_t numSlots;
} tScheduleSlotEntry;
#define SIZE_OF_SLOT_ENTRY 2

#define MAX_SLOT_ENTRIES 256
#define MAX_NODE_SLOT_ENTRIES 32 // How many slot entries can a node have

typedef struct {
    // These fields are also in the packet
    //uint8_t protocolVersion;
    uint8_t totalSchedulerPackets;
    uint8_t schedulerPacketIndex;
    uint8_t newNodeId;
    uint64_t newNodeUniqueId;
    uint8_t schedulerPeriodInFrames; // A schedule is sent every this many frames
    tSlot numSlotsPerFrame;
    // These fields are split over multiple packets
    uint16_t numSlotEntries; // This is a uint8_t in the packet
    tScheduleSlotEntry slotEntry[MAX_SLOT_ENTRIES];
} tMasterSchedulerState;

// This is mostly the same as the master but as a memory optimisation we
// only keep a record of our slots
typedef struct {
    bool schedulerInitialised;
    uint8_t expSchedulerPacketIndex;
    uint8_t totalSchedulerPackets;
    uint8_t newNodeId;
    uint64_t newNodeUniqueId;
    uint8_t schedulerPeriodInFrames; // A schedule is sent every this many frames
    tSlot numSlotsPerFrame;
    uint16_t numMySlotEntries;
    tSlot myStartSlots[MAX_NODE_SLOT_ENTRIES];
    uint8_t mySlotLengths[MAX_NODE_SLOT_ENTRIES];
} tSlaveSchedulerState;

#define ENTRIES_PER_PACKET ((PACKET_SIZE - 16) / SIZE_OF_SLOT_ENTRY)


void packNextSchedulerPacket(tMasterSchedulerState * schedule, tPacket * packet);
void unpackSchedulerPacket(uint8_t nodeId, tSlaveSchedulerState * schedule, tPacket * packet);

#endif