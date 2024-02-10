#ifndef SIMULATOR_H
#define SIMULATOR_H

#include "unity.h"
#include "microbus.h"
#include "packetChecker.h"

#define MAX_SIM_NODES 50
#define MAX_TX_DATA_BYTES 350
#define MAX_EVENTS (MAX_SIM_NODES * 5)

//#define FIRST_SLAVE_NODE_ID 0x01
#define MASTER_NODE_SIM_ID 0xFD

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
    tMasterNode master;
    tNodeInfo masterInfo;
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
tMasterNode * simCreateMaster(tSimulation * sim, uint64_t uniqueId);
tNode * simCreateSlave(tSimulation * sim, uint64_t uniqueId);
tPacket * simMasterCreateTxPacket(tSimulation * sim, tNodeIndex dstNodeSimId);
tPacket * simSlaveCreateTxPacket(tSimulation * sim, tNodeIndex srcNodeSimId);
void simulate(tSimulation * sim, uint64_t runTimeNs, bool start);
void simCheckAllPacketsReceived(tSimulation * sim);
    
// ======================================== //
// HAL replacements

void setPs(tSimulation * sim, tMasterNode * master, bool val);
void startSlaveTxRxDMA(tSimulation * sim, tNode * node, bool tx, uint8_t * txData, uint8_t * rxData, uint32_t numBytes);
void startMasterTxRxDMA(tSimulation * sim, tMasterNode * master, uint8_t * txData, uint8_t * rxData, uint32_t numBytes);
void stopTxRxDMA(tSimulation * sim, tNode * node);


#endif