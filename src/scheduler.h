#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "stdint.h"
#include "microbus_core.h"

// =========================== //
// Configurable

#define MAX_NEW_NODE_ENTRIES 16 // Master (~ 2 * 9 bytes)
#define MAX_SLOT_ENTRIES 256 // Master (512 bytes)
#define MAX_NODE_SLOT_ENTRIES 32 // How many slot entries can a node have (slave: 64 bytes)
#define MAX_NUM_SCHEDULER_PACKETS 250

// =========================== //


// NOTE: only 1 byte when total slots is 1024
// if more than 255 slots needed use multiple entries
typedef struct {
    uint8_t nodeId;
    uint8_t numSlots;
} tScheduleSlotEntry;

#define SIZE_OF_SLOT_ENTRY 2

typedef struct {
    uint8_t newNodeId;
    uint64_t newNodeUniqueId;
} tScheduleNewNodeEntry;

#define SIZE_OF_NEW_NODE_ENTRY 9

// Schedule packet;
// Fixed part:
    // uint8_t packetType
    // uint8_t version
    // uint8_t schedulerPacketIndex;
    // uint8_t finalSchedulerPacket;
    // uint8_t schedulerPeriodInFrames; // A schedule is sent every this many frames
    // uint16_t numSlots;
    // uint8_t numSlotEntries; // In this frame
    // uint8_t numNewNodes; // In this frame
// Dynamic part
    // numNewNodes * tScheduleNewNodeEntry
    // numSlotEntries * tScheduleSlotEntry
#define SCHEDULE_PACKET_FIXED_BYTES 9
    
// Data packet
// Header: (7 bytes / 200 bytes)
    // uint8_t packetType
    // uint8_t dstNodeId;
    // uint8_t srcNodeId;
    // uint8_t packetsCreatedLastFrame
    // uint8_t bufferLevelInPackets (rounded up)
    // uint16_t CRC
    

typedef struct {
    uint8_t schedulerPeriodInFrames; // A schedule is sent every this many frames
    tSlot numSlots; // Per frame
    uint8_t nodeSlots[MAX_NODES];
    uint8_t numRxNewNodes;
    uint64_t rxNewNodes[MAX_NEW_NODE_ENTRIES];
    // State during packing
    uint8_t schedulerPacketIndex;
    uint16_t slotEntryIndex;
    uint16_t newNodeIndex;
    uint16_t numSlotEntries;
    tScheduleSlotEntry slotEntry[MAX_SLOT_ENTRIES];
    uint8_t numNewNodes;
    tScheduleNewNodeEntry newNodeEntry[MAX_NEW_NODE_ENTRIES];
} tMasterSchedulerState;

// This is mostly the same as the master but as a memory optimisation we
// only keep a record of our slots
typedef struct {
    bool initialised;
    uint8_t schedulerPeriodInFrames; // A schedule is sent every this many frames
    tSlot numSlots;
    uint16_t numMySlotEntries;
    tSlot myStartSlots[MAX_NODE_SLOT_ENTRIES];
    uint8_t mySlotLengths[MAX_NODE_SLOT_ENTRIES];
    // State during unpacking
    uint8_t expSchedulerPacketIndex;
    tSlot slotIndex;
} tNodeSchedulerState;


bool txSchedulerPacket(tMasterSchedulerState * scheduler, tPacket * packet);
bool rxSchedulerPacket(tSlaveSchedulerState * scheduler, tPacket * packet, uint64_t unqiueNodeId, uint8_t * nodeId);

#endif