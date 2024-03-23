
#include "string.h"
#include "stdbool.h"
#include "stdint.h"
#include "stdio.h"
#include "stdlib.h"
#include "microbus.h"
#include "node.h"
#include "master.h"


void nodeTransferCompleteCb(tNode * node) {
    // TODO: control the psSignal ourselves to tell the master when we are finished
    // GPIO_set(psPin, LOW)
    
    // TODO: check if we need to stop dma
    // halStopNodeTxRxDMA
    
    // From being called to halStartNodeTxRxDMA we need to be as quick as possible!
    tPacket * nextRxPacket;
    tPacket * txPacket;
    processNodeRxAndGetTx(node, &txPacket, &nextRxPacket);
    
    if (txPacket == NULL) {
        node->halFns.startRxDMA((uint8_t *) nextRxPacket, PACKET_SIZE);
    } else {
        node->halFns.startTxRxDMA(txPacket->data, (uint8_t *) nextRxPacket, PACKET_SIZE);
    }
    
    // Signal that we are ready - when all nodes have released this signal the master can start
    // GPIO_set(psPin, HIGH) // open drain
}

void masterTransferCompleteCb(tMasterNode * master, bool startSequence) {
    tNodeIndex nextTxNodeId = masterSchedulerGetNextTxNodeId();
    
    processRxHeaders(node, &node->rxPackets[node->rxPacketsEnd]);

    tPacket * txPacket = masterProcessTx(master);
    txPacket->nextTxNodeId = nextTxNodeId;
    master->tx = true;
    // #ifdef LOGGING
    //     MB_PRINTF(master, "Master: mode:%u, slot:%u, numSlots:%u\n", master->mode, master->currentSlot, );
    // #endif
    
    // Master needs to tell nodes to start their DMA
    master->psVal = !master->psVal;
    halSetPs(master, master->psVal);
    
    // Wait 20us
    tPacket * rxPacket = allocateRxPacket(node);
    
    myAssert(master->rxPacketsReceived < MAX_RX_PACKETS, "Received more rx packets than buffer size");
    halStartMasterTxRxDMA(master, txPacket->data, (uint8_t *) &rxPacket, PACKET_SIZE);
    
    masterProcessRx(master);
}

void startMicrobus(tMasterNode * master) {
    masterTransferCompleteCb(master, true);
}
