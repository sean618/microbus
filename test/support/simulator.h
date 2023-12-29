#ifndef SIMULATOR_H
#define SIMULATOR_H

#include "unity.h"
#include "microbus.h"

#define MAX_SLAVES 256
#define MAX_EVENTS 5000
#define MAX_TX_DATA_BYTES 512

// ============================
// Events

typedef enum {
    SET_PS_HIGH, // 0
    SET_PS_LOW,
    SLAVE_TRANSFER_INITIATED_RX, // 2
    SLAVE_TRANSFER_INITIATED_TX_RX,
    MASTER_TRANSFER_STARTED, // 4
    MASTER_TRANSFER_FINISHED,
} eEvent;

typedef struct {
    uint64_t timeNs;
    uint32_t slaveSimIndex;
    eEvent type;
} tEvent;

// ============================

typedef struct {
    //uint64_t timeNs;
    tSlave * slave;
    bool tx;
    bool rx;
    uint8_t * txData;
    uint8_t * rxData;
    uint32_t numTransferBytes;
} tSimSlave;

typedef struct {
    //uint64_t timeNs;
    tMaster * master;
    bool tranferOccuring; // Master always transmits and receives
    uint8_t * txData;
    uint8_t * rxData;
    uint32_t numTransferBytes;
} tSimMaster;

typedef struct {
    tSimMaster master;
    tSimSlave slaves[MAX_SLAVES];
    uint8_t numSlaves;
    
    uint64_t timeNs;
    tEvent events[MAX_EVENTS];
    uint32_t numEvents;
    uint32_t eventCounter;
    
    uint8_t mosiData[MAX_TX_DATA_BYTES]; // Master -> slave
    uint32_t mosiDataBytes;
    uint8_t misoData[MAX_TX_DATA_BYTES]; // Slave -> master
    uint32_t misoDataBytes; 
    bool psWire;
} tSimulation;

tSimulation * simulate(tMaster * master, tSlave slaves[], uint32_t numSlaves, uint64_t runTimeNs);

// ======================================== //
// HAL replacements

void setPs(tMaster * master, bool val);
void startMasterTxRxDMA(tMaster * master, uint8_t * txData, uint8_t * rxData, uint32_t numBytes);
void startSlaveTxRxDMA(tSlave * slave, bool tx, uint8_t * txData, uint8_t * rxData, uint32_t numBytes);
void stopSlaveTxRxDMA(tSlave * slave);

#endif