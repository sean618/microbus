#ifndef MICROBUS_CORE_H
#define MICROBUS_CORE_H

#include "string.h"
#include "stdbool.h"
#include "stdint.h"

// User configurable

#define MAX_MASTER_TX_PACKETS 100
#define MAX_MASTER_RX_PACKETS 100
#define MAX_NODE_TX_PACKETS 100
#define MAX_NODE_RX_PACKETS 100

#define SPI_FREQ_MHZ 12 //12 - TODO: put back to 12MHZ
#define SPI_SLOT_TIME_US 200
#define DMA_WAIT_TIME_US 20

#define LOGGING 1

// Not configurable

#define MICROBUS_VERSION 1

#define SPI_SLOT_SIZE (SPI_FREQ_MHZ * SPI_SLOT_TIME_US / 8) // In bytes (12Mhz, 200us => 300 bytes)
#define PACKET_SIZE SPI_SLOT_SIZE

#define MAX_NODES 250 // Must be less than 255
typedef uint8_t tNodeIndex; // Node 0 not allowed, Node 255 is unused for new nodes to advertise
#define UNUSED_NODE_ID 0xFE // Used for signalling when newNodeId packets can be sent
#define INVALID_NODE_ID 0xFF // Shouldn't ever be used

// typedef uint16_t tSlot;
// #define MAX_SLOTS 1024 // 1024*0.2ms => 204ms

typedef enum {EMPTY_PACKET, DATA_PACKET, NEW_NODE_REQUEST_PACKET, NEW_NODE_RESPONSE_PACKET} tPacketType;

#define PACKET_DATA_SIZE (PACKET_SIZE-9)
typedef struct {
    // THIS MUST END UP PACKED - only use uint8_t !
    uint8_t nextTxNodeId; // Master only
    uint8_t nodeId;
    uint8_t protocolVersionAndPacketType;
    uint8_t dataSize1;
    uint8_t dataSize2;
    uint8_t ackSeqNum; // If from the master than this is actually the ackSeqNum for the nextTxNode
    uint8_t txSeqNum;
    uint8_t data[PACKET_DATA_SIZE];
    uint8_t crc1;
    uint8_t crc2;
} tPacket;

#define NEW_NODE_RESPONSE_ENTRY_SIZE 9 // uint64_t uniqueId; uint8_t nodeId;
#define NEW_NODE_REQUEST_ENTRY_SIZE 10 // uint64_t uniqueId, uint16_t checkSum

#define MB_PRINTF(node, str, ...) printf("%010ld: " str, node->simTimeNs, __VA_ARGS__)

#endif