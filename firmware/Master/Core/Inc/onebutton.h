#ifndef INC_ONEBUTTON_H_
#define INC_ONEBUTTON_H_

#include "stm32f1xx_hal.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    ONEBUTTON_ACTIVE_HIGH = 0U,
    ONEBUTTON_ACTIVE_LOW  = 1U
} OneButtonActiveLevel_t;

typedef struct
{
    GPIO_TypeDef *port;
    uint16_t pin;
    OneButtonActiveLevel_t activeLevel;

    uint8_t rawPressed;
    uint8_t lastRawPressed;
    uint8_t stablePressed;

    uint32_t rawChangeTime;
    uint32_t pressStartTime;
    uint32_t lastReleaseTime;

    uint32_t debounceMs;
    uint32_t longPressMs;
    uint32_t doubleClickMs;

    uint8_t pendingClickCount;
    uint8_t longPressReported;

    uint8_t pressedEvent;
    uint8_t releasedEvent;
    uint8_t clickEvent;
    uint8_t doubleClickEvent;
    uint8_t longPressEvent;
} OneButton_t;

/*
 * Khoi tao nut.
 *
 * activeLevel:
 *   ONEBUTTON_ACTIVE_LOW  : nut noi GND, GPIO pull-up
 *   ONEBUTTON_ACTIVE_HIGH : nut noi VCC, GPIO pull-down
 */
void OneButton_Init(OneButton_t *button,
                    GPIO_TypeDef *port,
                    uint16_t pin,
                    OneButtonActiveLevel_t activeLevel);

/*
 * Goi lien tuc trong while(1).
 * Khong dung HAL_Delay() ben trong thu vien.
 */
void OneButton_Tick(OneButton_t *button);

/* Cau hinh thoi gian. */
void OneButton_SetDebounceMs(OneButton_t *button,
                             uint32_t debounceMs);

void OneButton_SetLongPressMs(OneButton_t *button,
                              uint32_t longPressMs);

void OneButton_SetDoubleClickMs(OneButton_t *button,
                                uint32_t doubleClickMs);

/* Trang thai hien tai sau khi debounce. */
uint8_t OneButton_IsPressed(const OneButton_t *button);

/*
 * Cac ham event chi tra ve 1 mot lan,
 * sau khi doc se tu xoa event.
 */
uint8_t OneButton_WasPressed(OneButton_t *button);
uint8_t OneButton_WasReleased(OneButton_t *button);
uint8_t OneButton_WasClicked(OneButton_t *button);
uint8_t OneButton_WasDoubleClicked(OneButton_t *button);
uint8_t OneButton_WasLongPressed(OneButton_t *button);

#ifdef __cplusplus
}
#endif

#endif /* INC_ONEBUTTON_H_ */
