#ifndef MICROBUS_H
#define MICROBUS_H

#include "string.h"
#include "stdbool.h"
#include "stdint.h"

#define MICROBUS_VERSION 0

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

#define MAX_TX_PACKETS 100
#define MAX_RX_PACKETS 100


#define MAX_NODES 250 // Must be less than 255
typedef uint8_t tNodeIndex; // Node 0 not allowed, Node 255 is unused for new nodes to advertise
#define MASTER_NODE_ID 0x00
#define FIRST_SLAVE_NODE_ID 0x01
#define INVALID_NODE_ID 0xFF

#define SPI_FREQ_MHZ 1 //12 - TODO: put back to 12MHZ
#define SPI_SLOT_TIME_US 200
#define SPI_SLOT_SIZE (SPI_FREQ_MHZ * SPI_SLOT_TIME_US / 8) // In bytes (12Mhz, 200us => 300 bytes)
#define PACKET_SIZE SPI_SLOT_SIZE

#define DMA_WAIT_TIME_US 20

typedef uint16_t tSlot;
#define MAX_SLOTS 1024 // 1024*0.2ms => 204ms
#define MAX_SLOT_ENTRIES 256




typedef struct {
    uint64_t nodeUniqueId;
} tEnumPacket;


typedef struct {
    tNodeIndex dstNodeId;
    tNodeIndex srcNodeId;
    uint8_t data[PACKET_SIZE-2];
} tPacket;

// Ideally move this into microbus.c rather than exposing the entire state

typedef struct {
    tNodeIndex nodeId;
    
    tSlot currentSlot;
    bool tx;
    
    tSlot numSlots;
    uint32_t numTxPackets;
    tPacket txPacket[MAX_TX_PACKETS];
    uint32_t txPacketsSent;
    uint32_t numRxPackets;
    tPacket rxPacket[MAX_RX_PACKETS];
    uint32_t rxPacketsReceived;
    
    // Master only
    bool psVal; // TODO: needs a better name
    // Slave only
    tSlot txSlot; // TODO: placeholder
    
    tNodeIndex simId; // Simulation only
} tNode;


// HAL Callbacks
void transferCompleteCb(tNode * node); // HAL SPI callback
//void nodeTransferInitiatedCb();
void psLineInterrupt(tNode * node, bool psVal); // EXTI interrupt


void myAssert(uint8_t predicate, char * msg);
void start(tNode * master);

void addTxPacket(tNode * node, tNodeIndex dstNodeId, tPacket * packet);
tPacket * getTxPacket(tNode * node);
void removeTxPacket(tNode * node);
void addRxPacket(tNode * node, tPacket * packet);

#endif