#ifndef TEST_LCD_STM32F1XX_HAL_H
#define TEST_LCD_STM32F1XX_HAL_H
#include <stdint.h>
#include <stddef.h>
typedef struct { uint8_t unused; } I2C_HandleTypeDef;
typedef enum { HAL_OK = 0, HAL_ERROR = 1, HAL_BUSY = 2, HAL_TIMEOUT = 3 } HAL_StatusTypeDef;
#define I2C_FLAG_BUSY 1U
uint32_t HAL_GetTick(void);
void HAL_Delay(uint32_t delay);
uint32_t TestLCD_GetFlag(I2C_HandleTypeDef *handle, uint32_t flag);
#define __HAL_I2C_GET_FLAG(handle, flag) TestLCD_GetFlag((handle), (flag))
HAL_StatusTypeDef HAL_I2C_Master_Transmit(I2C_HandleTypeDef *handle,
    uint16_t address, uint8_t *data, uint16_t count, uint32_t timeout);
HAL_StatusTypeDef HAL_I2C_IsDeviceReady(I2C_HandleTypeDef *handle,
    uint16_t address, uint32_t trials, uint32_t timeout);
#endif
