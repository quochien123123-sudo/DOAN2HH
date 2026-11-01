/* Host tests for the REAL slave lcd_i2c.c with a minimal scripted HAL.
 * Model: HAL transmit can return after requesting STOP while physical BUSY
 * remains set briefly. This proves the driver path, not a hardware diagnosis.
 */
#include "lcd_i2c.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

I2C_HandleTypeDef hi2c1;
static uint32_t tick, busyPolls, flagReads, writes, probes, firstProbeTick;
static uint32_t transientBusy, stuckBusy, txFailure;
static uint16_t presentAddress, lastWriteAddress;
static uint8_t sent[64][12], sentLengths[64];

uint32_t HAL_GetTick(void) { return tick++; }
void HAL_Delay(uint32_t delay) { tick += delay; if (!stuckBusy) busyPolls = 0U; }
uint32_t TestLCD_GetFlag(I2C_HandleTypeDef *handle, uint32_t flag)
{
    assert(handle == &hi2c1 && flag == I2C_FLAG_BUSY);
    assert(++flagReads < 10000U); /* A regression must fail instead of hanging tests. */
    if (stuckBusy) return 1U;
    if (busyPolls) { --busyPolls; return 1U; }
    return 0U;
}
HAL_StatusTypeDef HAL_I2C_IsDeviceReady(I2C_HandleTypeDef *handle,
    uint16_t address, uint32_t trials, uint32_t timeout)
{
    assert(handle == &hi2c1 && trials == 1U && timeout == 3U);
    if (!probes) firstProbeTick = tick;
    ++probes;
    return address == presentAddress ? HAL_OK : HAL_ERROR;
}
HAL_StatusTypeDef HAL_I2C_Master_Transmit(I2C_HandleTypeDef *handle,
    uint16_t address, uint8_t *data, uint16_t count, uint32_t timeout)
{
    assert(handle == &hi2c1 && count <= 12U && timeout == 3U);
    lastWriteAddress = address;
    if (writes < 64U) {
        memcpy(sent[writes], data, count);
        sentLengths[writes] = (uint8_t)count;
    }
    ++writes;
    if (txFailure) return HAL_ERROR;
    /* The bundled STM32F1 HAL returns without waiting for STOP/BUSY to clear. */
    busyPolls = transientBusy ? 2U : 0U;
    return HAL_OK;
}
static void reset_fake(uint16_t address)
{
    tick = busyPolls = flagReads = writes = probes = firstProbeTick = 0U;
    transientBusy = stuckBusy = txFailure = 0U;
    presentAddress = address;
    lastWriteAddress = 0U;
    memset(sent, 0, sizeof(sent));
    memset(sentLengths, 0, sizeof(sentLengths));
}
static void check_nibble(uint32_t index, uint8_t nibble)
{
    assert(sentLengths[index] == 3U);
    assert(sent[index][0] == (uint8_t)(nibble | 0x08U));
    assert(sent[index][1] == (uint8_t)(nibble | 0x0CU));
    assert(sent[index][2] == (uint8_t)(nibble | 0x08U));
}
static void check_command(uint32_t index, uint8_t command)
{
    assert(sentLengths[index] == 6U);
    assert(sent[index][0] == (uint8_t)((command & 0xF0U) | 0x08U));
    assert(sent[index][3] == (uint8_t)((command << 4) | 0x08U));
}
int main(int argc, char **argv)
{
    uint32_t before, elapsed;
    if (argc == 2 && strcmp(argv[1], "--reproduce-original") == 0) {
        reset_fake(0x27U << 1);
        transientBusy = 1U;
        LCD_Init();
        printf("original transient-BUSY reproduction: ready=%u writes=%lu probes=%lu\n",
               (unsigned)LCD_IsReady(), (unsigned long)writes, (unsigned long)probes);
        assert(LCD_IsReady() == 0U && writes == 4U);
        return 0;
    }
    reset_fake(0x27U << 1);
    transientBusy = 1U;
    LCD_Init();
    assert(LCD_IsReady() == 1U && writes == 8U && probes == 1U);
    assert(firstProbeTick >= 50U && lastWriteAddress == (0x27U << 1));
    check_nibble(0U, 0x30U); check_nibble(1U, 0x30U);
    check_nibble(2U, 0x30U); check_nibble(3U, 0x20U);
    check_command(4U, 0x28U); check_command(5U, 0x0CU);
    check_command(6U, 0x06U); check_command(7U, 0x01U);
    LCD_SetLine(0U, "N1 R:ON RF:OK");
    LCD_Process();
    assert(LCD_IsReady() == 1U && writes == 9U && sentLengths[8] == 12U);
    LCD_Process(); /* A second immediate update must tolerate a transient STOP. */
    assert(LCD_IsReady() == 1U && writes == 10U);
    tick = UINT32_MAX - 1U;
    busyPolls = 2U;
    LCD_Process();
    assert(LCD_IsReady() == 1U && writes == 11U);
    puts("PASS transient BUSY after successful writes; init sequence and incremental updates");
    puts("PASS transient BUSY wait across the uint32_t tick wrap");

    reset_fake(0x3FU << 1);
    LCD_Init();
    assert(LCD_IsReady() == 1U && probes == 2U && firstProbeTick >= 50U);
    assert(lastWriteAddress == (0x3FU << 1));
    puts("PASS fallback address 0x3F after missing 0x27");

    stuckBusy = 1U;
    before = tick;
    LCD_SetLine(0U, "changed");
    LCD_Process();
    elapsed = tick - before;
    assert(LCD_IsReady() == 0U && elapsed <= 5U);
    before = writes;
    LCD_Process();
    assert(writes == before);
    puts("PASS stuck BUSY expires bounded wait and disables display, no repeated transfers");

    reset_fake(0x27U << 1);
    stuckBusy = 1U;
    LCD_Init();
    assert(LCD_IsReady() == 0U && probes == 0U && writes == 0U && tick <= 55U);
    puts("PASS stuck BUSY during init is bounded after power-on delay");

    reset_fake(0x27U << 1);
    LCD_Init();
    assert(LCD_IsReady() == 1U);
    txFailure = 1U;
    LCD_SetLine(0U, "HAL error");
    LCD_Process();
    assert(LCD_IsReady() == 0U);
    before = writes;
    LCD_Process();
    assert(writes == before);
    puts("PASS HAL transmit error disables display and preserves error isolation");

    reset_fake(0U); /* No responding backpack. */
    LCD_Init();
    assert(LCD_IsReady() == 0U && probes == 2U && writes == 0U);
    puts("PASS no responding address stays disabled");
    return 0;
}
