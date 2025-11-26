// Copyright (c) 2025 Sean Bremner
// Licensed under the MIT License. See LICENSE file for details.

#include <stdio.h>
#include "../src/microbus.h"
#include "test_basic.h"

void print_bytes(const uint8_t* data, int len) {
    for (int i = 0; i < len; ++i)
        printf("[%d] = 0x%02X\n", i, data[i]);
}

void testMicrobus();
void testScheduler();
void testNetworkManager();
void testTxManager();

FILE * logfile;
bool loggingEnabled = true;
uint64_t cycleIndex = 0;
uint64_t wCycleIndex = 0;


int main() {
    if (MICROBUS_LOGGING) {
        logfile = fopen("log.txt", "w");
        fprintf(logfile, "Start\n");
    }
    MB_PRINTF("Test\n");

    // testScheduler();
    testNetworkManager();
    testTxManager();

    test_packets_to_from_each_node(2, 1);
    test_packets_to_from_each_node(10-1, 1);
    test_packets_to_from_each_node(2, 100);
    test_packets_to_from_each_node(4, 50);

    testMicrobus();

    if (MICROBUS_LOGGING) {
        fprintf(logfile, "End\n");
        fclose(logfile);
    }
}