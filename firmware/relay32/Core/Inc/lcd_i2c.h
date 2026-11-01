#ifndef INC_LCD_I2C_H_
#define INC_LCD_I2C_H_
#include "stm32f1xx_hal.h"
#include <stdint.h>
/* HD44780 16x2 + PCF8574, autodetect address 0x27 or 0x3F. */
void LCD_Init(void);
uint8_t LCD_IsReady(void);
void LCD_SetLine(uint8_t row, const char *text);
/* At most one changed character per call. Never call while an RF reply is pending. */
void LCD_Process(void);
#endif
