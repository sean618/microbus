
/* =======================================
    Microbus master setup

This file contains some example code for using the microbus over 
spi on some STM32 MCUs.

Hardware setup:
    - SPI master configured:
        - Motorola, 16 bits, MSB first
        - CPOL - low
        - CPHA - 1 edge
        - CRC 16 bits Enabled (X0 + X1 + X2, NSS software)
        - Tx DMA normal - half word
        - Rx DMA normal - half word
    - GPIO output (PS pin)
    - A timer set to count every 200us

 ======================================= */

#include "../src/microbus.h"
#include "../src/master.h"

#define RESET_SPI_COUNT 50

typedef struct {
    tMaster * master;
    SPI_HandleTypeDef *hspi;
    uint32_t spiIndex;
    GPIO_TypeDef * psGpioGroup;
    uint16_t psGpioPin;
    TIM_HandleTypeDef * usTimer;

    // Stats
    uint32_t countSinceHeardNode;
    uint32_t lastNumSpiCallbacks; 
    uint32_t numSpiCallbacks; 
    uint32_t resetSpiCount;
    uint32_t numSpiResets;
} tSpiMaster;

tSpiMaster spiMaster;

void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *hspi) {
    HAL_SPI_TxRxCpltCallback(hspi);
}

void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *hspi) {
    myAssert(hspi == spiMaster.hspi, "");
    bool crcError = (HAL_SPI_GetError(hspi) == HAL_SPI_ERROR_CRC);
    spiMaster.numSpiCallbacks++;

    if (spiMaster.master->activeNodes.numNodes	> 0) {
        spiMaster.countSinceHeardNode = 0;
    } else {
        spiMaster.countSinceHeardNode++;
    }

    if (spiMaster.countSinceHeardNode > RESET_SPI_COUNT) {
        spiMaster.countSinceHeardNode = 0;
        // TODO: only valid for SPI1
        if (spiMaster.spiIndex == 1) {
            __HAL_RCC_SPI1_FORCE_RESET();
            __HAL_RCC_SPI1_RELEASE_RESET();
        } else {
            myAssert(spiMaster.spiIndex == 2, "");
            __HAL_RCC_SPI2_FORCE_RESET();
            __HAL_RCC_SPI2_RELEASE_RESET();
        }
        myAssert(HAL_OK == HAL_SPI_Init(spiMaster.hspi), "");
    }

    // Set pin to tell nodes a transaction is about to start
    HAL_GPIO_WritePin(spiMaster.psGpioGroup, spiMaster.psGpioPin, GPIO_PIN_SET);

    // Get our next data ptrs to use
    tPacket * masterTxPacket = NULL;
    tPacket * masterRxPacket = NULL;
    masterDualChannelPipelinedPreProcess(spiMaster.master, &masterTxPacket, &masterRxPacket, crcError);

    // Wait to give nodes a chance to start their TxRx DMAs
    delayUs(spiMaster.usTimer, 40); // TODO- check

    if (hspi->hdmatx->State != HAL_DMA_STATE_READY) {
        // If TX DMA not ready then still set the PS line but don't actually send any data
        delayUs(spiMaster.usTimer, 35);
        HAL_GPIO_WritePin(spiMaster.psGpioGroup, spiMaster.psGpioPin, GPIO_PIN_RESET);
        // Now process the packet
        masterDualChannelPipelinedPostProcess(spiMaster.master);
        return;
    }

    // Start transaction
    __HAL_SPI_CLEAR_CRCERRFLAG(hspi);
    volatile int res = HAL_SPI_TransmitReceive_DMA(hspi, (uint8_t *) masterTxPacket, (uint8_t *) masterRxPacket, (MB_PACKET_SIZE-1)/2);
    myAssert(res == HAL_OK, "SPI TxRx failed");

    // Reset PS line ready for next transaction
    HAL_GPIO_WritePin(spiMaster.psGpioGroup, spiMaster.psGpioPin, GPIO_PIN_RESET);

    // Now process the packet
    masterDualChannelPipelinedPostProcess(spiMaster.master);
}


void hwMasterInit(
        tMaster * master,
        SPI_HandleTypeDef *hspi,
        uint32_t spiIndex,
        GPIO_TypeDef * psGpioGroup,
        uint16_t psGpioPin,
        TIM_HandleTypeDef * usTimer) {
    memset(&spiMaster, 0, sizeof(spiMaster));
    spiMaster.master = master;
    spiMaster.hspi = hspi;
    spiMaster.spiIndex = spiIndex;
    spiMaster.psGpioGroup = psGpioGroup;
    spiMaster.psGpioPin = psGpioPin;
    spiMaster.usTimer = usTimer;
}

volatile uint16_t tmpBytes[3];
uint64_t stateNotReadyCount = 0;

void timerCallback() {
    masterUpdateTimeUs(spiMaster.master, 200);

    if (spiMaster.lastNumSpiCallbacks == spiMaster.numSpiCallbacks) {
        spiMaster.resetSpiCount++;
    } else {
        spiMaster.resetSpiCount = 0;
        spiMaster.lastNumSpiCallbacks = spiMaster.numSpiCallbacks;
    }

    if (spiMaster.resetSpiCount > 10) {
        spiMaster.resetSpiCount = 0;
        spiMaster.numSpiResets++;
    	if (spiMaster.hspi->hdmatx->State == HAL_DMA_STATE_READY) {
        	stateNotReadyCount = 0;
            // myAssert(spiMaster.hspi->hdmatx->State == HAL_DMA_STATE_READY);
			volatile int res = HAL_SPI_TransmitReceive_DMA(spiMaster.hspi, (uint8_t *) &tmpBytes[0], (uint8_t *) &tmpBytes[1], 1);
			myAssert(res == HAL_OK, "SPI restart failed");
    	} else {
        	stateNotReadyCount++;
    	}
    }
}

void hwMasterStart() {
    volatile int res = HAL_SPI_TransmitReceive_DMA(spiMaster.hspi, (uint8_t *) &tmpBytes[0], (uint8_t *) &tmpBytes[1], 1);
    myAssert(res == HAL_OK, "");
}


