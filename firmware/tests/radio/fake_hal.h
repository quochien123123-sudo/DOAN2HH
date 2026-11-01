#ifndef TEST_RADIO_FAKE_HAL_H
#define TEST_RADIO_FAKE_HAL_H
#include <stdint.h>
typedef struct { int unused; } SPI_HandleTypeDef;
typedef enum { HAL_OK, HAL_ERROR } HAL_StatusTypeDef;
typedef enum { GPIO_PIN_RESET, GPIO_PIN_SET } GPIO_PinState;
#define LORA_NSS_GPIO_Port ((void *)1)
#define LORA_NSS_Pin 1U
#define LORA_RST_GPIO_Port ((void *)2)
#define LORA_RST_Pin 2U
HAL_StatusTypeDef HAL_SPI_Transmit(SPI_HandleTypeDef *, uint8_t *, uint16_t, uint32_t);
HAL_StatusTypeDef HAL_SPI_TransmitReceive(SPI_HandleTypeDef *, uint8_t *, uint8_t *, uint16_t, uint32_t);
void HAL_GPIO_WritePin(void *, uint16_t, GPIO_PinState);
void HAL_Delay(uint32_t);
uint32_t HAL_GetTick(void);
#endif
