#include "lora.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
SPI_HandleTypeDef hspi1;
static uint8_t regmap[128], fifo[256];
static uint32_t ticks;
static unsigned failNext, failFifoWrite, badConfig, resetLow;
static unsigned transfers;

uint32_t HAL_GetTick(void) { return ticks; }
void HAL_Delay(uint32_t ms) { ticks += ms; }
void HAL_GPIO_WritePin(void *port, uint16_t pin, GPIO_PinState state)
{
    (void)pin;
    if (port == LORA_RST_GPIO_Port)
    {
        if (state == GPIO_PIN_RESET) resetLow = 1U;
        else if (resetLow)
        {
            memset(regmap, 0, sizeof regmap);
            memset(fifo, 0, sizeof fifo);
            regmap[0x42] = 0x12;
            resetLow = 0U;
        }
    }
}
HAL_StatusTypeDef HAL_SPI_Transmit(SPI_HandleTypeDef *spi, uint8_t *tx,
                                    uint16_t len, uint32_t timeout)
{
    uint8_t address = tx[0] & 0x7FU;
    (void)spi;
    assert(timeout <= 5U && len >= 2U);
    transfers++;
    if (failNext) { failNext = 0U; return HAL_ERROR; }
    if (address == 0U && failFifoWrite) { failFifoWrite = 0U; return HAL_ERROR; }
    for (unsigned i = 1U; i < len; ++i)
    {
        if (address == 0U) fifo[regmap[0x0D]++] = tx[i];
        else if (address == 0x12U) regmap[address] &= (uint8_t)~tx[i];
        else regmap[address] = tx[i];
    }
    return HAL_OK;
}
HAL_StatusTypeDef HAL_SPI_TransmitReceive(SPI_HandleTypeDef *spi, uint8_t *tx,
                                    uint8_t *rx, uint16_t len, uint32_t timeout)
{
    uint8_t address = tx[0] & 0x7FU;
    (void)spi;
    assert(timeout <= 5U && len >= 2U);
    transfers++;
    if (failNext) { failNext = 0U; return HAL_ERROR; }
    rx[0] = 0U;
    for (unsigned i = 1U; i < len; ++i)
        rx[i] = address == 0U ? fifo[regmap[0x0D]++] : regmap[address];
    if (badConfig && address == 0x1DU) rx[1] ^= 1U;
    return HAL_OK;
}
static void rxPacket(const uint8_t *data, uint8_t length, uint8_t flags)
{
    memcpy(fifo, data, length);
    regmap[0x10] = 0U;
    regmap[0x13] = length;
    regmap[0x1C] = 0x40U;
    regmap[0x19] = (uint8_t)(int8_t)-8;
    regmap[0x1A] = 100U;
    regmap[0x12] = flags;
    ticks += 2U;
    LoRa_Process();
}
int main(void)
{
    uint8_t payload[34], result[128];
    for (unsigned i = 0U; i < sizeof payload; ++i) payload[i] = (uint8_t)i;
    assert(LoRa_Init() && LoRa_IsReady() && regmap[1] == 0x8DU);
    uint32_t before = ticks;
    assert(LoRa_SendAsync(payload, sizeof payload));
    assert(LoRa_IsBusy() && LoRa_TxResult() == 0 && ticks == before);
    assert(!memcmp(payload, fifo, sizeof payload));
    regmap[0x12] = 8U;
    ticks += 2U;
    LoRa_Process();
    assert(!LoRa_IsBusy() && LoRa_TxResult() == 1 && regmap[1] == 0x8DU);
    puts("PASS async TX completes only on TxDone; no foreground wait");

    rxPacket(payload, sizeof payload, 0x40U);
    assert(!LoRa_SendAsync(payload, sizeof payload));
    assert(LoRa_ReadPacket(result, 33U) == 0U && loraRxDropCount == 1U);
    assert(LoRa_ReadPacket(result, sizeof result) == 0U);
    rxPacket(payload, sizeof payload, 0x40U);
    assert(LoRa_ReadPacket(result, sizeof result) == sizeof payload);
    assert(!memcmp(payload, result, sizeof payload));
    assert(LoRa_GetLastSNR() == -8 && LoRa_GetLastRSSI() == -66);
    puts("PASS unread RX retained, over-capacity rejected, SNR quarter-dB preserved");

    rxPacket(payload, sizeof payload, 0x60U);
    assert(!LoRa_ReadPacket(result, sizeof result) && loraCrcErrorCount == 1U);
    regmap[0x1C] = 0U; regmap[0x12] = 0x40U; ticks += 2U; LoRa_Process();
    assert(!LoRa_ReadPacket(result, sizeof result) && loraCrcErrorCount == 2U);
    puts("PASS CRC errors and CRC-absent explicit frames rejected");

    failNext = 1U; ticks += 2U; LoRa_Process();
    assert(!LoRa_IsReady() && LoRa_TxResult() == -1);
    assert(LoRa_Init());
    failFifoWrite = 1U;
    assert(!LoRa_SendAsync(payload, sizeof payload));
    assert(!LoRa_IsReady() && !LoRa_IsBusy() && regmap[1] != 0x8BU);
    puts("PASS SPI/FIFO failure aborts, never falsely accepted as TX");

    assert(LoRa_Init());
    assert(LoRa_SendAsync(payload, sizeof payload));
    ticks += 500U; LoRa_Process();
    assert(!LoRa_IsBusy() && !LoRa_IsReady() && LoRa_TxResult() == -1);
    assert(loraTxTimeoutCount == 1U);
    puts("PASS TX deadline is bounded at 500ms");

    assert(LoRa_Init());
    regmap[1] = 0U; ticks += 1000U; LoRa_Process();
    assert(!LoRa_IsReady());
    badConfig = 1U;
    assert(!LoRa_Init() && !LoRa_IsReady());
    badConfig = 0U;
    assert(LoRa_Init());
    printf("PASS radio brownout/config mismatch detected; total SPI calls %u\n", transfers);
    return 0;
}
