#include "string.h"
#include "stdbool.h"
#include "stdint.h"

#define MAX_TX_PACKETS 10
#define MAX_RX_PACKETS 10

typedef struct {
    uint32_t val;
} tPacket;

typedef struct {
    uint32_t numTxPackets;
    tPacket txPacket[MAX_TX_PACKETS];
    uint32_t numRxPackets;
    tPacket rxPacket[MAX_RX_PACKETS];
} tCommon;

typedef struct {
    tCommon common;
} tMaster;

typedef struct {
    tCommon common;
} tSlave;


void addTxPacket(tCommon * common, tPacket * packet);
tPacket * getTxPacket(tCommon * common);
void removeTxPacket(tCommon * common);
void addRxPacket(tCommon * common, tPacket * packet);