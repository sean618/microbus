

/* =======================================
    Microbus node/slave setup

This file contains some example code for using the microbus over 
spi on some STM32 MCUs.

Hardware setup:
    - SPI slave configured:
        - Motorola, 16 bits, MSB first
        - CPOL - low
        - CPHA - 1 edge
        - CRC 16 bits Enabled (X0 + X1 + X2, NSS software)
        - Tx DMA normal - half word
        - Rx DMA normal - half word
    - External interrupt for PS GPIO pin (on rising edge)
    - A timer set to count every 200us

 ======================================= */


#include "../src/microbus.h"
#include "../src/node.h"


#define MAX_MICROBUS_TX_PACKETS 4
#define MAX_MICROBUS_RX_PACKETS 4

#define RESET_SPI_COUNT 50
tNode node;
tPacketEntry txPacketEntries[MAX_MICROBUS_TX_PACKETS];
uint8_t maxRxPacketEntries;
tPacketEntry rxPacketEntries[MAX_MICROBUS_RX_PACKETS];
tPacketEntry * rxPacketQueue[MAX_MICROBUS_RX_PACKETS];

// =======================================//

typedef struct {
    bool spiStarted;
    SPI_HandleTypeDef *hspi;
    uint32_t spiIndex;
    GPIO_TypeDef* misoGpioX;
    uint16_t misoGpioPin;
    TIM_HandleTypeDef * htim;
    uint16_t psGpioPin;
    uint32_t savedSpiGpioAlternate;
    // Stats
    uint64_t numSpiResets;
    uint64_t goodCycles;
    uint64_t totalCycles;
    uint32_t waitCycles;
} tSpiNode;

static tSpiNode spiNode;

// =======================================//
// MISO pin setting
// Because this is a bus the MISO pin needs to be switched between
// normal MISO pin settings (when transmitting data) to high impedence 
// when not transmitting - otherwise other node's can't transmit.

static uint8_t getGPIOAlternateFunction(GPIO_TypeDef *GPIOx, uint16_t gpioPin) {
    uint32_t reg;
    uint8_t bitShift;
    uint8_t pinIndex = 0;

    while ((gpioPin >> pinIndex) != 1U) {
        pinIndex++;
    }

	#ifdef STM32F1
		if (pinIndex < 8) {
			reg = GPIOx->CRL;
			bitShift = pinIndex * 4U;
		} else {
			reg = GPIOx->CRH;
			bitShift = (pinIndex - 8U) * 4U;
		}
	#else
		// Determine which AFR register (AFR[0] or AFR[1]) based on pinIndex
		reg = GPIOx->AFR[(pinIndex / 8U)]; // 0 for pins 0..7, 1 for pins 8..15
		bitShift    = (pinIndex % 8U) * 4U; // Each pin AF uses 4 bits
	#endif

    // Extract the 4-bit mode/config value
    uint8_t af = (reg >> bitShift) & 0x0F;
    return af;
}

#ifdef STM32F1
static void restoreGPIOConfigSTM32F103(GPIO_TypeDef *GPIOx, uint16_t gpioPin, uint8_t savedConfig) {
    uint8_t pinIndex = 0;
    while ((gpioPin >> pinIndex) != 1U) {
        pinIndex++;
    }

    uint32_t *crReg;
    uint8_t bitShift;

    if (pinIndex < 8) {
        crReg = &GPIOx->CRL;
        bitShift = pinIndex * 4U;
    } else {
        crReg = &GPIOx->CRH;
        bitShift = (pinIndex - 8U) * 4U;
    }

    // Clear the old config bits
    *crReg &= ~(0x0F << bitShift);
    // Write the saved config back
    *crReg |= (savedConfig << bitShift);
}
#endif

static inline void setMisoPinMode(bool highImpedance) {
    GPIO_InitTypeDef gpioInit = {0};
    gpioInit.Pin = spiNode.misoGpioPin;
    gpioInit.Pull = GPIO_NOPULL;
    gpioInit.Speed = GPIO_SPEED_FREQ_HIGH;

	#ifndef STM32F1
		gpioInit.Alternate = spiNode.savedSpiGpioAlternate;
	#endif
    
    if (highImpedance) {
        gpioInit.Mode = GPIO_MODE_INPUT;
    } else {
        gpioInit.Mode = GPIO_MODE_AF_PP;
    }

    HAL_GPIO_Init(spiNode.misoGpioX, &gpioInit);

	#ifdef STM32F1
    	restoreGPIOConfigSTM32F103(spiNode.misoGpioX, spiNode.misoGpioPin, spiNode.savedSpiGpioAlternate);
	#endif
}

// =======================================//
// When a rising edge is detected on the PS line we need to start the next Tx & Rx SPI DMAs
// and process the previous packets.

void protocol_GPIO_EXTI_Rising_Callback() {
    if (spiNode.spiStarted == false) {
		return;
	}

    spiNode.totalCycles++;

    if (spiNode.waitCycles > 0) {
        spiNode.waitCycles--;
        return;
    }

    uint32_t error = HAL_SPI_GetError(spiNode.hspi);
    spiNode.hspi->ErrorCode = 0;
    bool crcError = (error == HAL_SPI_ERROR_CRC);

    // If last transaction didn't complete
    if ((spiNode.hspi->State != HAL_SPI_STATE_READY) ||
        	(spiNode.hspi->hdmatx->State != HAL_DMA_STATE_READY) ||
	    	(spiNode.hspi->hdmarx->State != HAL_DMA_STATE_READY) ||
        ((error != HAL_SPI_ERROR_NONE) && !crcError)) {
        // Abort transaction
        volatile HAL_StatusTypeDef res;
        res = HAL_SPI_Abort(spiNode.hspi);

        // Reset the SPI state
        if (spiNode.spiIndex == 1) {
            __HAL_RCC_SPI1_FORCE_RESET();
            __HAL_RCC_SPI1_RELEASE_RESET();
        } else {
            myAssert(spiNode.spiIndex == 2, "");
            __HAL_RCC_SPI2_FORCE_RESET();
            __HAL_RCC_SPI2_RELEASE_RESET();
        }
        res = HAL_SPI_Init(spiNode.hspi);
        myAssert(res == HAL_OK, "SPI restart failed");

        // Set Tx pin to high impedance so the other nodes can transmit
        setMisoPinMode(true);
        // Record failure
        spiNode.numSpiResets++;
        // Skip the next transaction to give the abort time enough time
        spiNode.waitCycles = 10;

    } else {
        // The CRC has to be manually cleared after each transaction
        __HAL_SPI_CLEAR_CRCERRFLAG(spiNode.hspi);
        // spiNode.lastTxRxCompleted = false;

        // Get our next data ptrs to use
        tPacket * nodeTxPacket = NULL;
        tPacket * nextNodeRxPacketMemory = NULL;
        nodeDualChannelPipelinedPreProcess(&node, &nodeTxPacket, &nextNodeRxPacketMemory, crcError);

        // Start next transaction
        if (nodeTxPacket) {
            setMisoPinMode(false);
            volatile int res = HAL_SPI_TransmitReceive_DMA(spiNode.hspi, (uint8_t *) nodeTxPacket, (uint8_t *) nextNodeRxPacketMemory, (MB_PACKET_SIZE-1)/2);
            myAssert(res == HAL_OK, "SPI next TxRx failed");
        } else {
            // Not our turn to transmit so only receive
            // Set Tx pin to high impedance so the other nodes can transmit
            setMisoPinMode(true);
            volatile int res = HAL_SPI_Receive_DMA(spiNode.hspi, (uint8_t *) nextNodeRxPacketMemory, (MB_PACKET_SIZE-1)/2);
            myAssert(res == HAL_OK, "SPI next Rx failed");
        }

        spiNode.goodCycles++;
        nodeDualChannelPipelinedPostProcess(&node);
    }
}

void protocol_TIM_PeriodElapsedCallback() {
    nodeUpdateTimeUs(&node, 200);
}

// =======================================//

void protocolInit(uint64_t uniqueId, SPI_HandleTypeDef *hspi,  uint32_t spiIndex, GPIO_TypeDef* misoGpioX, uint16_t misoGpioPin, TIM_HandleTypeDef *htim, uint16_t psGpioPin) {
    memset(&spiNode, 0, sizeof(spiNode));
    spiNode.hspi = hspi;
    spiNode.spiIndex = spiIndex;
    spiNode.misoGpioX = misoGpioX;
    spiNode.misoGpioPin = misoGpioPin;
    spiNode.htim = htim;
    spiNode.psGpioPin = psGpioPin;
    // When we start this interrupt might have already triggered and we only want to trigger at exactly the start of the frame
    // So wait for at least the next interrupt - then the data should be good
    spiNode.waitCycles = 10;

    spiNode.savedSpiGpioAlternate = getGPIOAlternateFunction(misoGpioX, misoGpioPin);
    nodeInit(&node, uniqueId, MAX_MICROBUS_TX_PACKETS, txPacketEntries, MAX_MICROBUS_RX_PACKETS, rxPacketEntries, rxPacketQueue);

    volatile HAL_StatusTypeDef result = HAL_TIM_Base_Start_IT(htim);
    myAssert(result == HAL_OK, "Starting timer failed");
    spiNode.spiStarted = true;
    setMisoPinMode(true);

    node.initialised = true;
    node.nodeId = 1;
}

// =======================================//
// Callbacks

#ifdef STM32F0
    __weak void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
        if (GPIO_Pin == spiNode.psGpioPin) {
            protocol_GPIO_EXTI_Rising_Callback();
        }
    }
#else
    __weak void HAL_GPIO_EXTI_Rising_Callback(uint16_t GPIO_Pin) {
        if (GPIO_Pin == spiNode.psGpioPin) {
            protocol_GPIO_EXTI_Rising_Callback();
        }
    }
#endif


__weak void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
    if (htim == spiNode.htim) {
        protocol_TIM_PeriodElapsedCallback();
    }
}

