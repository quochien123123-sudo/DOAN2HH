#include "lcd_i2c.h"
#include <string.h>
extern I2C_HandleTypeDef hi2c1;
static uint16_t address;
static uint8_t ready, cursor;
static char desired[2][16], shown[2][16];

static uint8_t write_bytes(uint8_t *bytes, uint16_t count)
{
    if (!ready) return 0U;
    if (__HAL_I2C_GET_FLAG(&hi2c1, I2C_FLAG_BUSY) ||
        HAL_I2C_Master_Transmit(&hi2c1, address, bytes, count, 3U) != HAL_OK) {
        ready = 0U; /* Stop on first fault; do not repeat hundreds of failed writes. */
        return 0U;
    }
    return 1U;
}

static void nibble_bytes(uint8_t value, uint8_t rs, uint8_t *out)
{
    uint8_t bits = (uint8_t)((value & 0xF0U) | 0x08U | rs);
    out[0] = bits;
    out[1] = bits | 0x04U;
    out[2] = bits;
}

static void byte_bytes(uint8_t value, uint8_t rs, uint8_t *out)
{
    nibble_bytes(value, rs, out);
    nibble_bytes((uint8_t)(value << 4), rs, out + 3);
}

static uint8_t command(uint8_t value)
{
    uint8_t bytes[6];
    byte_bytes(value, 0U, bytes);
    return write_bytes(bytes, sizeof(bytes));
}

void LCD_Init(void)
{
    static const uint8_t addresses[] = {0x27U, 0x3FU};
    uint8_t i, bytes[3];
    ready = 0U;
    address = 0U;
    memset(desired, ' ', sizeof(desired));
    memset(shown, 0, sizeof(shown));
    cursor = 0U;
    if (__HAL_I2C_GET_FLAG(&hi2c1, I2C_FLAG_BUSY)) return;
    for (i = 0U; i < sizeof(addresses); ++i) {
        if (HAL_I2C_IsDeviceReady(&hi2c1, (uint16_t)addresses[i] << 1, 1U, 3U) == HAL_OK) {
            address = (uint16_t)addresses[i] << 1;
            break;
        }
    }
    if (!address) return;
    ready = 1U;
    HAL_Delay(50U); /* Startup only; no conversion/display delays in the main loop. */
    for (i = 0U; i < 3U; ++i) {
        nibble_bytes(0x30U, 0U, bytes);
        if (!write_bytes(bytes, sizeof(bytes))) return;
        HAL_Delay(i == 0U ? 5U : 1U);
    }
    nibble_bytes(0x20U, 0U, bytes);
    if (!write_bytes(bytes, sizeof(bytes))) return;
    if (!command(0x28U) || !command(0x0CU) || !command(0x06U) || !command(0x01U)) return;
    HAL_Delay(2U);
}

uint8_t LCD_IsReady(void) { return ready; }

void LCD_SetLine(uint8_t row, const char *text)
{
    uint8_t col;
    if (row > 1U || text == NULL) return;
    for (col = 0U; col < 16U; ++col) {
        if (*text) desired[row][col] = *text++;
        else desired[row][col] = ' ';
    }
}

void LCD_Process(void)
{
    uint8_t count, row, col, bytes[12];
    if (!ready) return;
    for (count = 0U; count < 32U; ++count) {
        row = cursor / 16U;
        col = cursor % 16U;
        cursor = (uint8_t)((cursor + 1U) % 32U);
        if (desired[row][col] == shown[row][col]) continue;
        byte_bytes((uint8_t)(0x80U | (row ? 0x40U : 0U) | col), 0U, bytes);
        byte_bytes((uint8_t)desired[row][col], 1U, bytes + 6);
        if (write_bytes(bytes, sizeof(bytes))) shown[row][col] = desired[row][col];
        return;
    }
}
