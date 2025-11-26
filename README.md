# Microbus Protocol Library

A lightweight embedded communication protocol implementing a master-slave bus architecture for reliable multi-node communication. Designed for real-time embedded systems with support for up to 64 nodes.

It has been used on the STM32F0 range running at 10Mbps with 6MB of RAM.

## Core Architecture

**Master-Slave Model**: One master node coordinates communication with up to 64 slave nodes. The master controls all bus timing and scheduling—nodes only transmit when allocated a time slot by the master.

**Key Design Goals**:
- Reliable packet delivery using a sliding window protocol
- Dynamic node discovery and allocation (for plug and play operation)
- Low-latency operation suitable for real-time embedded systems (optimized for SPI/serial links)

## Module Overview

| Module | Purpose |
|--------|---------|
| `microbus.h` | Core definitions: packet structures, constants, circular buffer macros |
| `master.c/h` | Master node coordination—ties together scheduling, network management, Tx/Rx |
| `masterTx.c/h` | Master transmission logic and packet preparation |
| `masterRx.c/h` | Master receive processing with validation and acknowledgment handling |
| `node.c/h` | Slave node implementation—joining, Tx/Rx handling |
| `scheduler.c/h` | Time-slot allocation deciding which nodes transmit when |
| `txManager.c/h` | Sliding window reliable delivery with retransmission support |
| `rxManager.c/h` | Receive buffer management using circular queues |
| `networkManager.c/h` | Node join/leave handling via TTL-based membership |
| `common.c` | Shared utilities: queue operations, debug printing |

## Protocol Features

**Packet Types**: Data packets, empty/ack packets, new-node request/response, and master reset packets. Each packet carries scheduling information (next nodes to transmit) and acknowledgment sequence numbers.

**Reliable Delivery**: All packets will arrive without loss and in order. It uses a retransmission sliding window of size 4 with sequence numbers. Packets are retained for retransmission until acknowledged.

**Node Discovery**: Unallocated nodes send requests with their 64-bit unique ID during designated "unallocated slots." The master assigns node IDs and broadcasts responses. Nodes maintain their position via periodic transmission or time out.


## Typical Data Flow

1. Master broadcasts schedule: "Node 3, you transmit next"
2. Node 3 sends data packet with its buffer level and ack for any master packets received
3. Master acknowledges Node 3's packet in subsequent transmissions
4. Scheduler rotates through active nodes, balancing data transfer and acknowledgments

## Building and Testing

Running `build_and_test.sh` will both build the code (using cmake) and then run the tests.

## Microcontroller Pin Configuration

The microbus uses 3-pin SPI (MOSI, MISO and SCK) along with an extra GPIO pin for the bus.

A timer is also needed to do some housekeeping and check the link is still active.

## Using the Microbus Protocol

The basics of how to use this protocol are described below. The `example_usage` directory contains `stm32_master.c` and `stm32_node.c` with more detailed examples for use on STM32 MCUs.

### Master

```c
// Indicate the start of a new packet 
HAL_GPIO_WritePin(spiMaster.psGpioGroup, spiMaster.psGpioPin, GPIO_PIN_SET);

// Get our next data ptrs to use
tPacket * masterTxPacket = NULL;
tPacket * masterRxPacket = NULL;
masterDualChannelPipelinedPreProcess(spiMaster.master, &masterTxPacket, &masterRxPacket, crcError);

// Give the nodes/slaves a chance to setup their SPI DMAs
delayUs(spiMaster.usTimer, 40);

// Start SPI Tx and Rx
HAL_SPI_TransmitReceive_DMA(hspi, (uint8_t *) masterTxPacket, (uint8_t *) masterRxPacket, (MB_PACKET_SIZE-1)/2);

// Reset PS line ready for next transaction
HAL_GPIO_WritePin(spiMaster.psGpioGroup, spiMaster.psGpioPin, GPIO_PIN_RESET);

// Now process the previous packet
masterDualChannelPipelinedPostProcess(spiMaster.master);
```

### Node

```c
// When a rising edge is detected on the PS line
void protocol_GPIO_EXTI_Rising_Callback() {
    // Get our next data ptrs to use
    tPacket * nodeTxPacket = NULL;
    tPacket * nextNodeRxPacketMemory = NULL;
    nodeDualChannelPipelinedPreProcess(&node, &nodeTxPacket, &nextNodeRxPacketMemory, crcError);
    
    if (nodeTxPacket) {
        // If transmitting
        // Set the MISO pin to normal and start the SPI Tx and Rx
        setMisoPinMode(false);
        HAL_SPI_TransmitReceive_DMA(spiNode.hspi, (uint8_t *) nodeTxPacket, (uint8_t *) nextNodeRxPacketMemory, (MB_PACKET_SIZE-1)/2);
    } else {
        // Not our turn to transmit so only receive
        // Set Tx pin to high impedance so the other nodes can transmit and start the SPI Rx
        setMisoPinMode(true);
        HAL_SPI_Receive_DMA(spiNode.hspi, (uint8_t *) nextNodeRxPacketMemory, (MB_PACKET_SIZE-1)/2);
    }
    
    // Now process the previous packet
    nodeDualChannelPipelinedPostProcess(&node);
}
```

## Adaptations

This protocol is fairly hardware agnostic, so it does not need to be over SPI—it could potentially be used over UART instead. All it really needs is:

- A master-to-node line
- A node-to-master line
- A line for signalling the start of a packet
- The node/slave data pins need to be put into high impedance mode when not transmitting

This protocol can also be adapted to use just a single data channel for both master and node transmissions. The code is mostly already there for this, but it hasn't been tested.

## License

Copyright (c) 2025 Sean Bremner  
Licensed under the MIT License. See LICENSE file for details.