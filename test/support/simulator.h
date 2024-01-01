#ifndef SIMULATOR_H
#define SIMULATOR_H

#include "unity.h"
#include "microbus.h"
#include "packetChecker.h"

#define MAX_SIM_NODES 50
#define MAX_TX_DATA_BYTES 256
#define MAX_EVENTS (MAX_SIM_NODES * 5)

// ============================
// Events

typedef enum {
    // SET_PS_HIGH, // 0
    // SET_PS_LOW,
    NODE_TRANSFER_INITIATED_RX, // 2
    NODE_TRANSFER_INITIATED_TX_RX,
    MASTER_TRANSFER_STARTED, // 4
    MASTER_TRANSFER_FINISHED,
} eEvent;

typedef struct {
    uint64_t timeNs;
    uint32_t nodeSimId;
    eEvent type;
} tEvent;

// ============================

typedef struct {
    //uint64_t timeNs;
    //tNode * node;
    bool valid;
    bool tx;
    bool rx;
    uint8_t * txData;
    uint8_t * rxData;
    uint32_t numTransferBytes;
} tNodeInfo;

typedef struct {
    tPacketChecker packetChecker;
    tNode nodes[MAX_SIM_NODES];
    tNodeInfo nodeInfo[MAX_SIM_NODES];
    uint8_t numNodes;
    
    uint64_t timeNs;
    tEvent events[MAX_EVENTS];
    uint32_t numEvents;
    uint32_t eventCounter;
    
    bool transferOccuring;
    uint8_t mosiData[MAX_TX_DATA_BYTES]; // Master -> node
    uint32_t mosiDataBytes;
    uint8_t misoData[MAX_TX_DATA_BYTES]; // Node -> master
    uint32_t misoDataBytes; 
    bool psWire;
} tSimulation;


void simInit(tSimulation * sim);
tNode * simCreateNode(tSimulation * sim, bool master);
tPacket * simCreatePacket(tSimulation * sim, tNodeIndex srcNodeSimId, tNodeIndex dstNodeSimId);
void simulate(tSimulation * sim, uint64_t runTimeNs);
void simCheckAllPacketsReceived(tSimulation * sim);
    
// ======================================== //
// HAL replacements

void setPs(tSimulation * sim, tNode * master, bool val);
void startTxRxDMA(tSimulation * sim, tNode * node, bool tx, uint8_t * txData, uint8_t * rxData, uint32_t numBytes);
void stopTxRxDMA(tSimulation * sim, tNode * node);


#endif