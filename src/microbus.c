#include "string.h"
#include "stdbool.h"
#include "stdint.h"
#include "stdio.h"

//#include "cexception.h"

#include "microbus.h"



void myAssert(uint8_t predicate, char * msg) {
    if (predicate == 0)
    {
        //Throw(1);
        printf("Assert: %s\n", msg);
        // uint8_t * ptr = NULL;
    	// *ptr = 0; // Hit debugger
        // return;
    }
}

tPacket emptyTxPacket;

void masterTransferCompleteCb(tMaster * master) {
    myAssert(master->common.txPacketsSent < MAX_TX_PACKETS, "Sent more tx packets than buffer size");
    myAssert(master->common.rxPacketsReceived < MAX_RX_PACKETS, "Received more rx packets than buffer size");
    
    // We increment until we reach a blank packet and we'll continue to send that black packet
    if (master->common.tx) {
        master->common.txPacketsSent++;
    }
    
    // Check if any data
    // TODO: Just checking if first byte is not zero - Change
    if (master->common.rxPacket[master->common.rxPacketsReceived].data[0] != 0xFF) {
        master->common.rxPacketsReceived++;
    }
    
    master->common.currentSlot++;
    if (master->common.currentSlot == master->common.numSlots) {
        master->common.currentSlot = 0;
    }
    master->common.tx = (master->common.txPacketsSent < master->common.numTxPackets);
        
    uint8_t * txData;
    if (master->common.tx) {
        txData = master->common.txPacket[master->common.txPacketsSent].data;
    } else {
        txData = emptyTxPacket.data;
    }
    
    // Tell slaves to start their SPI DMA
    master->psVal = !master->psVal;
    setPs(master, master->psVal);
    
    // Wait 20us
    
    // Start our SPI DMA 
    uint8_t * rxData = master->common.rxPacket[master->common.rxPacketsReceived].data;
    startMasterTxRxDMA(master, txData, rxData, PACKET_SIZE);
}

void psLineInterrupt(tSlave * slave, bool psVal) {
    myAssert(slave->common.txPacketsSent < MAX_TX_PACKETS, "Sent more tx packets than buffer size");
    myAssert(slave->common.rxPacketsReceived < MAX_RX_PACKETS, "Received more rx packets than buffer size");
    
    if (slave->common.tx) {
        slave->common.txPacketsSent++;
    }
    
    // Check if any data
    // TODO: Just checking if first byte is not zero - Change
    if (slave->common.rxPacket[slave->common.rxPacketsReceived].data[0] != 0xFF) {
        slave->common.rxPacketsReceived++;
        printf("Slave rx:%d\n", slave->common.rxPacket[slave->common.rxPacketsReceived].data[0]);
    }
    
    slave->common.currentSlot++;
    if (slave->common.currentSlot == slave->common.numSlots) {
        slave->common.currentSlot = 0;
    }
    
    // Transmit if it's our slot and we have data
    slave->common.tx = (slave->txSlot == slave->common.currentSlot)
        && (slave->common.txPacketsSent <= slave->common.numTxPackets);
        
    //printf("Slave:%d, txSlot:%d, sent:%d, num:%d\n", slave->simIndex, slave->common.currentSlot, slave->common.txPacketsSent, slave->common.numTxPackets);
        
    uint8_t * txData;
    if (slave->common.tx) {
        txData = slave->common.txPacket[slave->common.txPacketsSent].data;
    } else {
        txData = emptyTxPacket.data;
    }
    
    // 
    uint8_t * rxData = slave->common.rxPacket[slave->common.rxPacketsReceived].data;
    startSlaveTxRxDMA(slave, slave->common.tx, txData, rxData, PACKET_SIZE);
}

void start(tMaster * master) {
    memset(&emptyTxPacket.data[0], 0xFF, sizeof(emptyTxPacket.data));
    masterTransferCompleteCb(master);
}



void addTxPacket(tCommon * common, tPacket * packet) {
    if (common->numTxPackets < MAX_TX_PACKETS) {
        memcpy(&common->txPacket[common->numTxPackets], packet, sizeof(tPacket));
        common->numTxPackets++;
    } else {
        myAssert(0, "Tx buffer is full");
    }
}