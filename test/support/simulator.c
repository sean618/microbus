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
        // This will fire the interrupts on all slaves
        for (uint32_t i=0; i<sim.numSlaves; i++) {
            psLineInterrupt(sim.slaves[i].slave, sim.psWire);
        }
    }
}

static void slaveTransferInitiated(tEvent * event, bool tx) {
    if (sim.master.tranferOccuring) {
        // Master is already transmitting 
        //event->slave->partialRx = true; // TODO: work out how much is transferred
        myAssert(0, "Slave started receiving after master started the transfer");
        if (tx) {
            myAssert(0, "Slave started transmitting after master started the transfer");
        }
    }
    sim.slaves[event->slaveSimIndex].rx = true;
    sim.slaves[event->slaveSimIndex].tx = tx;
}

// At the start of the master transfer copy all master tx bytes into the MOSI buffer
// and OR in all the slaves tx bytes into the MISO buffer
static void masterTransferStarted(tEvent * event) {
    sim.master.tranferOccuring = true;
    
    // Set the bus to all high - as there will be pull-ups
    memset(&sim.misoData[0], 0xFF, sizeof(sim.misoData));
    memset(&sim.mosiData[0], 0xFF, sizeof(sim.mosiData));
    
    // Put data on bus - Master to mosi
    myAssert(sim.master.numTransferBytes < MAX_TX_DATA_BYTES, "Master Tx data exceeded limit");
    memcpy(sim.mosiData, sim.master.txData, sim.master.numTransferBytes);
    //sim.mosiDataBytes = sim.master.numTransferBytes;
    
    // Put data on bus - Slaves to miso
    for (uint32_t i=0; i<sim.numSlaves; i++) {
        if (sim.slaves[i].tx) {
            myAssert(sim.slaves[i].numTransferBytes < MAX_TX_DATA_BYTES, "Slave Tx data exceeded limit");
            for (uint32_t j=0; j<sim.slaves[i].numTransferBytes; j++) {
                sim.misoData[j] &= sim.slaves[i].txData[j];
            }
            //sim.misoDataBytes = MAX(sim.misoDataBytes, sim.slaves[i].numTransferBytes);
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
    
    // mosi to slaves
    for (uint32_t i=0; i<sim.numSlaves; i++) {
        if (sim.slaves[i].rx) {
            memcpy(sim.slaves[i].rxData, sim.mosiData, sim.slaves[i].numTransferBytes);
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
            slaveTransferInitiated(event, event->type == SLAVE_TRANSFER_INITIATED_TX_RX);
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

tSimulation * simulate(tMaster * master, tSlave slaves[], uint32_t numSlaves, uint64_t runTimeNs) {
    memset(&sim, 0, sizeof(sim));
    sim.numSlaves = numSlaves;
    for (uint32_t i=0; i<numSlaves; i++) {
        slaves[i].simIndex = i;
        memset(slaves[i].common.rxPacket[0].data, 0xFF, sizeof(slaves[i].common.rxPacket[0].data));
        sim.slaves[i].slave = &slaves[i];
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

// void pauseSlave(tSlave * slave, uint64_t pauseNs) {
//     sim.slaves[slave->simIndex].timeNs += pauseNs;
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
        printf("Time: %ld: Master sending packet data[0]: %d\n", event.timeNs, txData[0]);
    }
}

void startSlaveTxRxDMA(tSlave * slave, bool tx, uint8_t * txData, uint8_t * rxData, uint32_t numBytes) {
    tEvent event = {0};
    event.slaveSimIndex = slave->simIndex;
    tSimSlave * simSlave = &sim.slaves[event.slaveSimIndex];
    simSlave->txData = txData;
    simSlave->rxData = rxData;
    simSlave->numTransferBytes = numBytes;
    // Pause for 20us
    event.type = tx == 0 ? SLAVE_TRANSFER_INITIATED_RX : SLAVE_TRANSFER_INITIATED_TX_RX;
    event.timeNs = sim.timeNs + DMA_WAIT_TIME_US * 1000 - 100;
    addEvent(&event);
    
    if (txData[0] != 0xFF) {
        printf("Time: %ld: Slave %d, sending packet data[0]: %d\n", event.timeNs, event.slaveSimIndex, txData[0]);
    }
}

void stopSlaveTxRxDMA(tSlave * slave) {
    tSimSlave * simSlave = &sim.slaves[slave->simIndex];
    simSlave->tx = 0;
    simSlave->rx = 0;
}
