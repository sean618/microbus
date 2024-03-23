#ifndef MICROBUS_H
#define MICROBUS_H

#include "string.h"
#include "stdbool.h"
#include "stdint.h"
// #include "txManager.h"
// #include "scheduler.h"


// User configurable

// #define MAX_MASTER_TX_PACKETS 100 
// #define MAX_MASTER_RX_PACKETS 100
// #define MAX_NODE_TX_PACKETS 10 // 10 * 300 bytes => 3KB
// #define MAX_NODE_RX_PACKETS 10 // 10 * 300 bytes => 3KB
#ifndef DUAL_CHANNEL_MODE
#define DUAL_CHANNEL_MODE 1 // DUAL_CHANNEL means there is a channel from the master -> agents and from the agents -> master - SINGLE_CHANNEL means just one channel for master and agents 
#endif

#define DATA_RATE_MHZ 12 //12 - TODO: put back to 12MHZ
#define SLOT_TIME_US 200

#define MAX_LATENCY_MS 100 // This needs to be big enough to give every node a chance to transmit
#define MAX_SLOTS_PER_NODE_TX_CHANCE ((MAX_LATENCY_MS * 1000) / SLOT_TIME_US) // 100ms -> 500 slots before a node has to get a transmit slot (with 0.2ms slots)


#define LOGGING 1

// Not configurable

#define MICROBUS_VERSION 1

#define SLOT_SIZE (DATA_RATE_MHZ * SLOT_TIME_US / 8) // In bytes (12Mhz, 200us => 300 bytes)
#define PACKET_SIZE SLOT_SIZE

typedef uint8_t tNodeIndex; // Node 0 not allowed, Node 255 is unused for new nodes to advertise
#define MAX_NODES 250 // Must be less than 255
// #define MAX_AGENTS (MAX_NODES-1)
#define MASTER_NODE_ID 0
#define FIRST_AGENT_NODE_ID 1
#define UNALLOCATED_NODE_ID 0xFE // Used for signalling when newNodeId packets can be sent
#define INVALID_NODE_ID 0xFF // Shouldn't ever be used


// TODO: change empty packet to ACK PACKET - allowing master to ack multiple nodes
typedef enum {MASTER_DATA_PACKET, NODE_DATA_PACKET, /*EMPTY_PACKET,*/ NEW_NODE_REQUEST_PACKET, NEW_NODE_RESPONSE_PACKET, MAX_PACKET_TYPE} tPacketType;


#define NUM_TX_NODES_SCHEDULED (DUAL_CHANNEL_MODE ? 1 : 5) // This means up N nodes can transmit before the master transmits

#define MASTER_PACKET_DATA_SIZE (PACKET_SIZE-8-(2*NUM_TX_NODES_SCHEDULED)) // Either 10 or 18 byte header
typedef struct {
    // THIS MUST END UP PACKED - only use uint8_t !
    // These initial fields are read first independently of whether we need to process the rest of the packet
    // We necessarily want to CRC check the whole packet just for these fields so we use the checksum instead
    uint8_t nextTxNodeId[NUM_TX_NODES_SCHEDULED]; // Master only
    uint8_t nextTxNodeAckSeqNum[NUM_TX_NODES_SCHEDULED];
    uint8_t checkSum; // 
    // Remaining packet - only need to process if the packet is for us
    uint8_t dstNodeId;
    uint8_t data[MASTER_PACKET_DATA_SIZE];
} tMasterPacket;

#define NODE_PACKET_DATA_SIZE (PACKET_SIZE-9)
typedef struct {
    // THIS MUST END UP PACKED - only use uint8_t !
    uint8_t ackSeqNum;
    uint8_t srcNodeId;
    uint8_t bufferLevel;
    uint8_t data[NODE_PACKET_DATA_SIZE];
} tNodePacket;

typedef struct {
    // THIS MUST END UP PACKED - only use uint8_t !
    uint8_t protocolVersionAndPacketType;
    uint8_t txSeqNum;
    uint8_t dataSize1;
    uint8_t dataSize2;
    union {
        tMasterPacket master;
        tNodePacket node;
    };
    uint8_t crc1;
    uint8_t crc2;
} tPacket;




#define MB_PRINTF(node, str, ...) printf("%010ld: " str, node->simTimeNs, __VA_ARGS__)

#endif