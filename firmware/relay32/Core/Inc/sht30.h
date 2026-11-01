#ifndef INC_SHT30_H_
#define INC_SHT30_H_
#include "stm32f1xx_hal.h"
#include <stdint.h>
#define SHT30_ADDR (0x44U << 1)
/* No conversion wait in either call. Call Finish >=20ms after successful Start. */
HAL_StatusTypeDef SHT30_Start(void);
HAL_StatusTypeDef SHT30_Finish(int16_t *temperature10, uint8_t *humidity);
#endif
