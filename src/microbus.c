#include "string.h"
#include "stdbool.h"
#include "stdint.h"
#include "stdio.h"

//#include "cexception.h"

#include "microbus.h"


void halSetPs(tNode * master, bool val);
void halStartMasterTxRxDMA(tNode * master, uint8_t * txData, uint8_t * rxData, uint32_t numBytes);
void halStartNodeTxRxDMA(tNode * node, bool tx, uint8_t * txData, uint8_t * rxData, uint32_t numBytes);
void halStopNodeTxRxDMA(tNode * node);


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

void transferCompleteCb(tNode * node) {
    myAssert(node->txPacketsSent < MAX_TX_PACKETS, "Sent more tx packets than buffer size");
    myAssert(node->rxPacketsReceived < MAX_RX_PACKETS, "Received more rx packets than buffer size");
    
    if (node->nodeId != MASTER_NODE_ID) {
        // TODO: check if we need to stop dma
        // halStopNodeTxRxDMA
    }
    
    if (node->tx) {
        node->txPacketsSent++;
    }
    
    // Check if any data
    tPacket * packet = &node->rxPacket[node->rxPacketsReceived];
    tNodeIndex dstNodeId = packet->data[0];
    tNodeIndex srcNodeId = packet->data[1];
    
    // Check if any data
    // TODO: Just checking if first byte is not zero - Change
    if (dstNodeId == node->nodeId && srcNodeId != INVALID_NODE_ID && srcNodeId != node->nodeId) {
        //printf("Node rx:%d\n", node->rxPacket[node->rxPacketsReceived].data[0]);
        node->rxPacketsReceived++;
    }
    
    node->tx = (node->txPacketsSent < node->numTxPackets);
    
    // Slaves only transmit only if it's their slot
    if (node->nodeId != MASTER_NODE_ID) {
        node->tx = node->tx && (node->txSlot == node->currentSlot);
    }
    
    node->currentSlot++;
    if (node->currentSlot >= node->numSlots) {
        node->currentSlot = 0;
    }   
    //printf("Node:%d, txSlot:%d, sent:%d, num:%d\n", node->simId, node->currentSlot, node->txPacketsSent, node->numTxPackets);
        
    uint8_t * txData;
    if (node->tx) {
        txData = node->txPacket[node->txPacketsSent].data;
    } else {
        txData = emptyTxPacket.data;
    }
    
    // Master needs to tell slaves to start their DMA
    if (node->nodeId == MASTER_NODE_ID) {
        node->psVal = !node->psVal;
        halSetPs(node, node->psVal);
        
        // Wait 20us
    }
    
    // 
    uint8_t * rxData = node->rxPacket[node->rxPacketsReceived].data;
    halStartNodeTxRxDMA(node, node->tx, txData, rxData, PACKET_SIZE);
}

void psLineInterrupt(tNode * node, bool psVal) {
    myAssert(node->simId == node->nodeId, "");
    transferCompleteCb(node);
}

void start(tNode * master) {
    memset(&emptyTxPacket.data[0], 0xFF, sizeof(emptyTxPacket.data));
    transferCompleteCb(master);
}

void addTxPacket(tNode * node, tNodeIndex dstNodeId, tPacket * packet) {
    packet->data[0] = dstNodeId;
    packet->data[1] = node->nodeId;
    if (node->numTxPackets < MAX_TX_PACKETS) {
        memcpy(&node->txPacket[node->numTxPackets], packet, sizeof(tPacket));
        node->numTxPackets++;
    } else {
        myAssert(0, "Tx buffer is full");
    }
}