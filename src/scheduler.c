#include "string.h"
#include "stdbool.h"
#include "stdint.h"
#include "stdio.h"

#include "scheduler.h"
//#include "cexception.h"
#include "microbus_core.h"
#include "microbus.h"

// void schedule() {
//     // Combine Created packets with buffer level and try and schedule that rate
    
//     total_slots_required = 
    
    
// }

void updateSchedule(tMasterSchedulerState * scheduler) {
    // Any new nodes heard whilst we are sending scheduler packets are stored separately
    // Now we've finished sending scheduler packets, copy across all the heard new nodes,
    // assign them node ids, and update the schedule
    
    uint32_t rxNodeIndex = 0;
    for (uint32_t nodeId=0; (nodeId < MAX_NODES) && (rxNodeIndex < scheduler->numRxNewNodes); nodeId++) {
        // If unused slot
        if (scheduler->nodeSlots[nodeId] == 0) {
            scheduler->nodeSlots[nodeId] = 1; // Give it 1 slot to begin with
            scheduler->newNodeEntry[rxNodeIndex].newNodeUniqueId = scheduler->rxNewNodes[rxNodeIndex];
            scheduler->newNodeEntry[rxNodeIndex].newNodeId = nodeId;
            rxNodeIndex++;
        }
    }
    myAssert(rxNodeIndex == scheduler->numRxNewNodes, "Max slots reached - No space for new nodes");
    scheduler->numNewNodes = rxNodeIndex;
    scheduler->numRxNewNodes = 0;
    
    // Convert our node slots into a schedule
    // This is just to allow slots to not have to be contiguous
    uint32_t entryIndex = 0;
    for (uint32_t i=0; i<MAX_SLOTS; i++) {
        if (scheduler->nodeSlots[i] > 0) {
            scheduler->slotEntry[entryIndex].nodeId = i;
            scheduler->slotEntry[entryIndex].numSlots = scheduler->nodeSlots[i];
            entryIndex++;
        }
    }
    scheduler->numSlotEntries = entryIndex;
    scheduler->schedulerPeriodInFrames = 1; // TODO
}



