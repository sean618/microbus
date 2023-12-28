#include "string.h"
#include "stdbool.h"
#include "stdint.h"



#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

#define MAX_TX_PACKETS 10
#define MAX_RX_PACKETS 10


#define MAX_NODES 250 // Must be less than 255

#define MAX_SLOT_ENTRIES 256

#define SPI_FREQ_MHZ 1 //12 - TODO: put back to 12MHZ
#define SPI_SLOT_TIME_US 200
#define SPI_SLOT_SIZE (SPI_FREQ_MHZ * SPI_SLOT_TIME_US / 8) // In bytes (12Mhz, 200us => 300 bytes)

#define PACKET_SIZE SPI_SLOT_SIZE

#define DMA_WAIT_TIME_US 20

typedef struct {
    uint8_t data[PACKET_SIZE];
} tPacket;

// Ideally move this into microbus.c rather than exposing the entire state

typedef struct {
    uint32_t currentSlot;
    bool tx;
    uint32_t numTxPackets;
    tPacket txPacket[MAX_TX_PACKETS];
    uint32_t txPacketsSent;
    uint32_t numRxPackets;
    tPacket rxPacket[MAX_RX_PACKETS];
    uint32_t rxPacketsReceived;
} tCommon;

typedef struct {
    bool psVal; // TODO: needs a better name
    tCommon common;
} tMaster;

typedef struct {
    uint32_t simIndex; // Simulation only
    uint32_t txSlot; // TODO: placeholder
    tCommon common;
} tSlave;


// HAL Callbacks
void masterTransferCompleteCb(tMaster * master); // HAL SPI callback
//void slaveTransferInitiatedCb();
void psLineInterrupt(tSlave * slave, bool psVal); // EXTI interrupt

// HAL functions
void setPs(tMaster * master, bool val);
void startMasterTxRxDMA(tMaster * master, uint8_t * txData, uint8_t * rxData, uint32_t numBytes);
void startSlaveTxRxDMA(tSlave * slave, bool tx, uint8_t * txData, uint8_t * rxData, uint32_t numBytes);
void stopSlaveTxRxDMA(tSlave * slave);


void myAssert(uint8_t predicate, char * msg);
void start(tMaster * master);

void addTxPacket(tCommon * common, tPacket * packet);
tPacket * getTxPacket(tCommon * common);
void removeTxPacket(tCommon * common);
void addRxPacket(tCommon * common, tPacket * packet);