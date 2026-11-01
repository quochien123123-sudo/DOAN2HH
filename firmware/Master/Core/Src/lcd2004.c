#include "lcd2004.h"
#include <string.h>

/*
 * Common PCF8574 backpack mapping:
 * P0=RS, P1=RW, P2=EN, P3=Backlight, P4..P7=D4..D7
 */

#define LCD_RS          0x01U
#define LCD_EN          0x04U
#define LCD_BACKLIGHT   0x08U

static I2C_HandleTypeDef *lcdHi2c = NULL;
static uint16_t lcdAddress = 0U;  /* HAL address: 7-bit << 1 */
static uint8_t lcdReady = 0U;

static HAL_StatusTypeDef LCD2004_WriteRaw(uint8_t data)
{
    if ((lcdHi2c == NULL) || (lcdAddress == 0U))
    {
        return HAL_ERROR;
    }

    HAL_StatusTypeDef result = HAL_I2C_Master_Transmit(lcdHi2c, lcdAddress, &data, 1U, 3U);
    if (result != HAL_OK) {
        lcdReady = 0U;
        lcdAddress = 0U; /* Abort further display writes after the first error. */
    }
    return result;
}

static void LCD2004_PulseEnable(uint8_t data)
{
    uint8_t value;

    value = data | LCD_EN;
    (void)LCD2004_WriteRaw(value);

    /*
     * At 100 kHz, the I2C transfer time itself is already much longer
     * than the HD44780 minimum Enable pulse width.
     */
    value = data & (uint8_t)(~LCD_EN);
    (void)LCD2004_WriteRaw(value);
}

static void LCD2004_Write4Bits(uint8_t nibble, uint8_t control)
{
    uint8_t data;

    data = (nibble & 0xF0U) |
           control |
           LCD_BACKLIGHT;

    (void)LCD2004_WriteRaw(data);
    LCD2004_PulseEnable(data);
}

static void LCD2004_Send(uint8_t value, uint8_t mode)
{
    LCD2004_Write4Bits(value & 0xF0U, mode);
    LCD2004_Write4Bits((uint8_t)(value << 4), mode);
}

static void LCD2004_Command(uint8_t command)
{
    LCD2004_Send(command, 0U);

    if ((command == 0x01U) || (command == 0x02U))
    {
        HAL_Delay(2);
    }
}

static void LCD2004_Data(uint8_t data)
{
    LCD2004_Send(data, LCD_RS);
}

uint8_t LCD2004_Init(I2C_HandleTypeDef *hi2c)
{
    static const uint8_t candidateAddress[] =
    {
        0x27U,
        0x3FU
    };

    uint8_t i;

    lcdHi2c = hi2c;
    lcdAddress = 0U;
    lcdReady = 0U;

    if (lcdHi2c == NULL)
    {
        return 0U;
    }

    /* The PCF8574/LCD supply must settle before the first address probe. */
    HAL_Delay(50);

    for (i = 0U; i < 2U; i++)
    {
        uint16_t halAddress =
            (uint16_t)(candidateAddress[i] << 1);

        if (HAL_I2C_IsDeviceReady(lcdHi2c,
                                  halAddress,
                                  1U,
                                  3U) == HAL_OK)
        {
            lcdAddress = halAddress;
            break;
        }
    }

    if (lcdAddress == 0U)
    {
        return 0U;
    }

    LCD2004_Write4Bits(0x30U, 0U);
    HAL_Delay(5);

    LCD2004_Write4Bits(0x30U, 0U);
    HAL_Delay(5);

    LCD2004_Write4Bits(0x30U, 0U);
    HAL_Delay(2);

    LCD2004_Write4Bits(0x20U, 0U);
    HAL_Delay(2);

    LCD2004_Command(0x28U);  /* 4-bit mode */
    LCD2004_Command(0x0CU);  /* display ON, cursor OFF */
    LCD2004_Command(0x06U);  /* entry mode */
    LCD2004_Clear();

    lcdReady = lcdAddress != 0U;
    return lcdReady;
}

void LCD2004_Clear(void)
{
    if (lcdAddress == 0U)
    {
        return;
    }

    LCD2004_Command(0x01U);
    HAL_Delay(2);
}

void LCD2004_SetCursor(uint8_t row, uint8_t col)
{
    static const uint8_t rowAddress[4] =
    {
        0x00U,
        0x40U,
        0x14U,
        0x54U
    };

    if ((lcdAddress == 0U) || (row > 3U) || (col > 19U))
    {
        return;
    }

    LCD2004_Command((uint8_t)(0x80U |
                    (rowAddress[row] + col)));
}

void LCD2004_Print(const char *text)
{
    if ((lcdAddress == 0U) || (text == NULL))
    {
        return;
    }

    while (lcdAddress && *text != '\0')
    {
        LCD2004_Data((uint8_t)*text);
        text++;
    }
}

void LCD2004_PrintLine(uint8_t row, const char *text)
{
    char line[21];
    uint8_t i;

    if (row > 3U)
    {
        return;
    }

    for (i = 0U; i < 20U; i++)
    {
        line[i] = ' ';
    }

    line[20] = '\0';

    if (text != NULL)
    {
        for (i = 0U; (i < 20U) && (text[i] != '\0'); i++)
        {
            line[i] = text[i];
        }
    }

    LCD2004_SetCursor(row, 0U);
    LCD2004_Print(line);
}

uint8_t LCD2004_IsReady(void)
{
    return lcdReady;
}

uint8_t LCD2004_GetAddress7bit(void)
{
    return (uint8_t)(lcdAddress >> 1);
}
