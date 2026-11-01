#include "sht30.h"
extern I2C_HandleTypeDef hi2c1;

static uint8_t crc8(const uint8_t *data, uint8_t size)
{
    uint8_t crc = 0xFFU, bit;
    while (size--) {
        crc ^= *data++;
        for (bit = 0U; bit < 8U; ++bit)
            crc = (crc & 0x80U) ? (uint8_t)((crc << 1) ^ 0x31U) : (uint8_t)(crc << 1);
    }
    return crc;
}

HAL_StatusTypeDef SHT30_Start(void)
{
    uint8_t command[2] = {0x24U, 0x00U};
    if (__HAL_I2C_GET_FLAG(&hi2c1, I2C_FLAG_BUSY)) return HAL_BUSY;
    return HAL_I2C_Master_Transmit(&hi2c1, SHT30_ADDR, command, sizeof(command), 3U);
}

HAL_StatusTypeDef SHT30_Finish(int16_t *temperature10, uint8_t *humidity)
{
    uint8_t data[6];
    uint32_t rawT, rawH, h;
    int32_t t;
    if (temperature10 == NULL || humidity == NULL) return HAL_ERROR;
    if (__HAL_I2C_GET_FLAG(&hi2c1, I2C_FLAG_BUSY)) return HAL_BUSY;
    if (HAL_I2C_Master_Receive(&hi2c1, SHT30_ADDR, data, sizeof(data), 3U) != HAL_OK)
        return HAL_ERROR;
    if (crc8(data, 2U) != data[2] || crc8(data + 3, 2U) != data[5]) return HAL_ERROR;
    rawT = ((uint32_t)data[0] << 8) | data[1];
    rawH = ((uint32_t)data[3] << 8) | data[4];
    /* Rounded fixed-point datasheet formula; no soft-float arithmetic needed. */
    t = (int32_t)((1750UL * rawT + 32767UL) / 65535UL) - 450L;
    h = (100UL * rawH + 32767UL) / 65535UL;
    if (t < -400L || t > 1250L || h > 100UL) return HAL_ERROR;
    *temperature10 = (int16_t)t;
    *humidity = (uint8_t)h;
    return HAL_OK;
}
