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
    
    // Tell nodes to start their SPI DMA
    master->psVal = !master->psVal;
    setPs(master, master->psVal);
    
    // Wait 20us
    
    // Start our SPI DMA 
    uint8_t * rxData = master->common.rxPacket[master->common.rxPacketsReceived].data;
    startMasterTxRxDMA(master, txData, rxData, PACKET_SIZE);
}

void psLineInterrupt(tNode * node, bool psVal) {
    myAssert(node->common.txPacketsSent < MAX_TX_PACKETS, "Sent more tx packets than buffer size");
    myAssert(node->common.rxPacketsReceived < MAX_RX_PACKETS, "Received more rx packets than buffer size");
    
    if (node->common.tx) {
        node->common.txPacketsSent++;
    }
    
    // Check if any data
    // TODO: Just checking if first byte is not zero - Change
    if (node->common.rxPacket[node->common.rxPacketsReceived].data[0] != 0xFF) {
        //printf("Node rx:%d\n", node->common.rxPacket[node->common.rxPacketsReceived].data[0]);
        node->common.rxPacketsReceived++;
    }
    
    // Transmit if it's our slot and we have data
    node->common.tx = (node->txSlot == node->common.currentSlot)
        && (node->common.txPacketsSent < node->common.numTxPackets);
    
    node->common.currentSlot++;
    if (node->common.currentSlot == node->common.numSlots) {
        node->common.currentSlot = 0;
    }   
    //printf("Node:%d, txSlot:%d, sent:%d, num:%d\n", node->simIndex, node->common.currentSlot, node->common.txPacketsSent, node->common.numTxPackets);
        
    uint8_t * txData;
    if (node->common.tx) {
        txData = node->common.txPacket[node->common.txPacketsSent].data;
    } else {
        txData = emptyTxPacket.data;
    }
    
    // 
    uint8_t * rxData = node->common.rxPacket[node->common.rxPacketsReceived].data;
    startNodeTxRxDMA(node, node->common.tx, txData, rxData, PACKET_SIZE);
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