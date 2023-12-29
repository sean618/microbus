#include "unity.h"
#include "simulator.h"

// TODO: what if the master transfer doesn't get enough bytes and therefore doesn't complete

tSimulation sim = {0};


// ======================================== //
// Keep events sorted by time
// All this memcpying and shifting everything is very inefficient if there are lost of events in the buffer
// Could improve by using a linked list or something else if we need to

static void addEvent(tEvent * event) {
    if (sim.numEvents == MAX_EVENTS) {
        myAssert(0, "Event buffer full\n");
        return;
    }
    // Find the position to insert it
    // Insert at the end as default
    uint32_t index = sim.numEvents; 
    // Now search to see if there is an earlier time to insert it
    for (uint32_t i=0; i<sim.numEvents; i++) {
        if (event->timeNs < sim.events[i].timeNs) {
            index = i;
            break;
        }
    }
    // Shift all events up
    for (int32_t j=sim.numEvents; j>index; j--) {
        memcpy(&sim.events[j], &sim.events[j-1], sizeof(tEvent));
    }
    // Insert new event
    memcpy(&sim.events[index], event, sizeof(tEvent));
    sim.numEvents++;
}

static bool popNextEvent(tEvent * event) {
    if (sim.numEvents == 0) {
        return false;
    }
    memcpy(event, &sim.events[0], sizeof(tEvent));
    // Shift all events down
    for (uint32_t i=1; i<sim.numEvents; i++) {
        memcpy(&sim.events[i-1], &sim.events[i], sizeof(tEvent));
    }
    sim.numEvents--;
    return true;
}


// ======================================== //

static void psSet(bool val) {
    myAssert(val != sim.psWire, "Setting PS wire but value not changing\n");
    if (val != sim.psWire) {
        sim.psWire = val;
        // This will fire the interrupts on all nodes
        for (uint32_t i=0; i<sim.numNodes; i++) {
            psLineInterrupt(sim.nodes[i].node, sim.psWire);
        }
    }
}

static void nodeTransferInitiated(tEvent * event, bool tx) {
    if (sim.master.tranferOccuring) {
        // Master is already transmitting 
        //event->node->partialRx = true; // TODO: work out how much is transferred
        myAssert(0, "Node started receiving after master started the transfer");
        if (tx) {
            myAssert(0, "Node started transmitting after master started the transfer");
        }
    }
    sim.nodes[event->nodeSimIndex].rx = true;
    sim.nodes[event->nodeSimIndex].tx = tx;
}

// At the start of the master transfer copy all master tx bytes into the MOSI buffer
// and OR in all the nodes tx bytes into the MISO buffer
static void masterTransferStarted(tEvent * event) {
    sim.master.tranferOccuring = true;
    
    // Set the bus to all high - as there will be pull-ups
    memset(&sim.misoData[0], 0xFF, sizeof(sim.misoData));
    memset(&sim.mosiData[0], 0xFF, sizeof(sim.mosiData));
    
    // Put data on bus - Master to mosi
    myAssert(sim.master.numTransferBytes < MAX_TX_DATA_BYTES, "Master Tx data exceeded limit");
    memcpy(sim.mosiData, sim.master.txData, sim.master.numTransferBytes);
    //sim.mosiDataBytes = sim.master.numTransferBytes;
    
    // Put data on bus - Nodes to miso
    for (uint32_t i=0; i<sim.numNodes; i++) {
        if (sim.nodes[i].tx) {
            myAssert(sim.nodes[i].numTransferBytes < MAX_TX_DATA_BYTES, "Node Tx data exceeded limit");
            for (uint32_t j=0; j<sim.nodes[i].numTransferBytes; j++) {
                sim.misoData[j] &= sim.nodes[i].txData[j];
            }
            //sim.misoDataBytes = MAX(sim.misoDataBytes, sim.nodes[i].numTransferBytes);
            // TODO: assert if not enum packets
        }
    }
    
    // Add an event for when the transfer will be finished
    tEvent finishedEvent = {0};
    finishedEvent.type = MASTER_TRANSFER_FINISHED;
    finishedEvent.timeNs = sim.timeNs + SPI_SLOT_TIME_US * 1000;
    addEvent(&finishedEvent);
}

static void masterTransferFinished(tEvent * event) {
    // miso to master
    memcpy(sim.master.rxData, sim.misoData, sim.master.numTransferBytes);
    sim.misoDataBytes = 0;
    
    // mosi to nodes
    for (uint32_t i=0; i<sim.numNodes; i++) {
        if (sim.nodes[i].rx) {
            memcpy(sim.nodes[i].rxData, sim.mosiData, sim.nodes[i].numTransferBytes);
        }
    }
    sim.master.tranferOccuring = false;
    masterTransferCompleteCb(sim.master.master);
}


static void actionEvent(tEvent * event) {
    switch (event->type) {
        case SET_PS_HIGH:
        case SET_PS_LOW:
            psSet(event->type == SET_PS_HIGH);
            break;
        case SLAVE_TRANSFER_INITIATED_RX:
        case SLAVE_TRANSFER_INITIATED_TX_RX:
            nodeTransferInitiated(event, event->type == SLAVE_TRANSFER_INITIATED_TX_RX);
            break;
        case MASTER_TRANSFER_STARTED:
            masterTransferStarted(event);
            break;
        case MASTER_TRANSFER_FINISHED:
            masterTransferFinished(event);
            break;
        default:
            myAssert(0, "Unhandled event type");
            return;
    }
}

tSimulation * simulate(tMaster * master, tNode nodes[], uint32_t numNodes, uint64_t runTimeNs) {
    memset(&sim, 0, sizeof(sim));
    sim.numNodes = numNodes;
    for (uint32_t i=0; i<numNodes; i++) {
        nodes[i].simIndex = i;
        memset(nodes[i].common.rxPacket[0].data, 0xFF, sizeof(nodes[i].common.rxPacket[0].data));
        sim.nodes[i].node = &nodes[i];
    }
    memset(master->common.rxPacket[0].data, 0xFF, sizeof(master->common.rxPacket[0].data));
    sim.master.master = master;
    
    start(sim.master.master);
    
    uint64_t prevTimeNs = 0;
    while (sim.timeNs < runTimeNs) {
        tEvent event;
        bool validEvent = popNextEvent(&event);
        if (!validEvent) {
            // Finished - although shouldn't ever happen
            myAssert(0, "Events finished");
            break;
        }
        //printf("%d, Event %d, time: %ld\n", i, event.type, event.timeNs);
        myAssert(event.timeNs >= sim.timeNs, "Event time earlier than sim time!");
        sim.timeNs = event.timeNs;
        
        actionEvent(&event);
        sim.eventCounter++;
        
        
        // Add Tx packet
        // if () {
            
        // }
    }
    printf("Simulated %d events over %ldms\n", sim.eventCounter, sim.timeNs/1000000);
    return &sim;
}


// ======================================== //
// HAL replacements

// void pauseMaster(uint64_t pauseNs) {
//     sim.master.timeNs += pauseNs;
// }

// void pauseNode(tNode * node, uint64_t pauseNs) {
//     sim.nodes[node->simIndex].timeNs += pauseNs;
// }

void psSet(bool val);
void setPs(tMaster * master, bool val) {
    psSet(val);
    // tEvent event = {0};
    // event.type = val == 0 ? SET_PS_LOW : SET_PS_HIGH;
    // // Pause for 100ns
    // event.timeNs = sim.timeNs + 1;
    // addEvent(&event);
}

void startMasterTxRxDMA(tMaster * master, uint8_t * txData, uint8_t * rxData, uint32_t numBytes) {
    tEvent event = {0};
    sim.master.txData = txData;
    sim.master.rxData = rxData;
    sim.master.numTransferBytes = numBytes;
    event.type = MASTER_TRANSFER_STARTED;
    event.timeNs = sim.timeNs + DMA_WAIT_TIME_US * 1000;
    addEvent(&event);
    
    if (txData[0] != 0xFF) {
        printf("Time: %9ld: Master  : sending packet data[0]: %d\n", event.timeNs, txData[0]);
    }
}

void startNodeTxRxDMA(tNode * node, bool tx, uint8_t * txData, uint8_t * rxData, uint32_t numBytes) {
    tEvent event = {0};
    event.nodeSimIndex = node->simIndex;
    tSimNode * simNode = &sim.nodes[event.nodeSimIndex];
    simNode->txData = txData;
    simNode->rxData = rxData;
    simNode->numTransferBytes = numBytes;
    // Pause for 20us
    event.type = tx == 0 ? SLAVE_TRANSFER_INITIATED_RX : SLAVE_TRANSFER_INITIATED_TX_RX;
    event.timeNs = sim.timeNs + DMA_WAIT_TIME_US * 1000 - 100;
    addEvent(&event);
    
    if (txData[0] != 0xFF) {
        printf("Time: %9ld: Node %3d: sending packet data[0]: %d\n", event.timeNs, event.nodeSimIndex, txData[0]);
    }
}

void stopNodeTxRxDMA(tNode * node) {
    tSimNode * simNode = &sim.nodes[node->simIndex];
    simNode->tx = 0;
    simNode->rx = 0;
}
