#ifndef __LCD2004_H
#define __LCD2004_H

#include "main.h"
#include <stdint.h>

uint8_t LCD2004_Init(I2C_HandleTypeDef *hi2c);
void LCD2004_Clear(void);
void LCD2004_SetCursor(uint8_t row, uint8_t col);
void LCD2004_Print(const char *text);
void LCD2004_PrintLine(uint8_t row, const char *text);
uint8_t LCD2004_IsReady(void);
uint8_t LCD2004_GetAddress7bit(void);

#endif
