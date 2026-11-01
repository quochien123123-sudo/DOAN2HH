#include "onebutton.h"
#include <stddef.h>

#define ONEBUTTON_DEFAULT_DEBOUNCE_MS       40U
#define ONEBUTTON_DEFAULT_LONG_PRESS_MS     800U
#define ONEBUTTON_DEFAULT_DOUBLE_CLICK_MS   350U


static uint8_t OneButton_ReadPressed(const OneButton_t *button)
{
    GPIO_PinState pinState;

    if ((button == NULL) ||
        (button->port == NULL))
    {
        return 0U;
    }

    pinState =
        HAL_GPIO_ReadPin(button->port,
                         button->pin);

    if (button->activeLevel ==
        ONEBUTTON_ACTIVE_LOW)
    {
        return (pinState == GPIO_PIN_RESET) ?
               1U : 0U;
    }

    return (pinState == GPIO_PIN_SET) ?
           1U : 0U;
}


void OneButton_Init(OneButton_t *button,
                    GPIO_TypeDef *port,
                    uint16_t pin,
                    OneButtonActiveLevel_t activeLevel)
{
    uint8_t pressed;

    if ((button == NULL) ||
        (port == NULL))
    {
        return;
    }

    button->port = port;
    button->pin = pin;
    button->activeLevel = activeLevel;

    button->debounceMs =
        ONEBUTTON_DEFAULT_DEBOUNCE_MS;

    button->longPressMs =
        ONEBUTTON_DEFAULT_LONG_PRESS_MS;

    button->doubleClickMs =
        ONEBUTTON_DEFAULT_DOUBLE_CLICK_MS;

    pressed =
        OneButton_ReadPressed(button);

    button->rawPressed = pressed;
    button->lastRawPressed = pressed;
    button->stablePressed = pressed;

    button->rawChangeTime =
        HAL_GetTick();

    button->pressStartTime =
        HAL_GetTick();

    button->lastReleaseTime =
        HAL_GetTick();

    button->pendingClickCount = 0U;
    button->longPressReported = 0U;

    button->pressedEvent = 0U;
    button->releasedEvent = 0U;
    button->clickEvent = 0U;
    button->doubleClickEvent = 0U;
    button->longPressEvent = 0U;
}


void OneButton_SetDebounceMs(OneButton_t *button,
                             uint32_t debounceMs)
{
    if (button == NULL)
    {
        return;
    }

    button->debounceMs = debounceMs;
}


void OneButton_SetLongPressMs(OneButton_t *button,
                              uint32_t longPressMs)
{
    if (button == NULL)
    {
        return;
    }

    button->longPressMs = longPressMs;
}


void OneButton_SetDoubleClickMs(OneButton_t *button,
                                uint32_t doubleClickMs)
{
    if (button == NULL)
    {
        return;
    }

    button->doubleClickMs = doubleClickMs;
}


void OneButton_Tick(OneButton_t *button)
{
    uint32_t now;
    uint8_t rawPressed;

    if ((button == NULL) ||
        (button->port == NULL))
    {
        return;
    }

    now =
        HAL_GetTick();

    rawPressed =
        OneButton_ReadPressed(button);

    button->rawPressed =
        rawPressed;

    /*
     * Raw thay doi -> bat dau lai bo dem debounce.
     */
    if (rawPressed !=
        button->lastRawPressed)
    {
        button->lastRawPressed =
            rawPressed;

        button->rawChangeTime =
            now;
    }

    /*
     * Chi chap nhan trang thai moi neu giu on dinh
     * lien tuc du debounceMs.
     */
    if ((rawPressed !=
         button->stablePressed) &&
        ((now -
          button->rawChangeTime) >=
         button->debounceMs))
    {
        button->stablePressed =
            rawPressed;

        if (button->stablePressed)
        {
            button->pressedEvent = 1U;
            button->pressStartTime = now;
            button->longPressReported = 0U;
        }
        else
        {
            button->releasedEvent = 1U;
            button->lastReleaseTime = now;

            /*
             * Long press khong tinh la click.
             */
            if (!button->longPressReported)
            {
                button->pendingClickCount++;

                if (button->pendingClickCount >= 2U)
                {
                    button->doubleClickEvent = 1U;
                    button->pendingClickCount = 0U;
                }
            }
            else
            {
                button->pendingClickCount = 0U;
            }
        }
    }

    /*
     * Long press.
     */
    if (button->stablePressed &&
        (!button->longPressReported) &&
        ((now -
          button->pressStartTime) >=
         button->longPressMs))
    {
        button->longPressReported = 1U;
        button->longPressEvent = 1U;
        button->pendingClickCount = 0U;
    }

    /*
     * Single click chi duoc xac nhan sau khi het
     * cua so double-click.
     */
    if ((!button->stablePressed) &&
        (button->pendingClickCount == 1U) &&
        ((now -
          button->lastReleaseTime) >=
         button->doubleClickMs))
    {
        button->clickEvent = 1U;
        button->pendingClickCount = 0U;
    }
}


uint8_t OneButton_IsPressed(const OneButton_t *button)
{
    if (button == NULL)
    {
        return 0U;
    }

    return button->stablePressed;
}


static uint8_t OneButton_ConsumeEvent(uint8_t *eventFlag)
{
    uint8_t value;

    if (eventFlag == NULL)
    {
        return 0U;
    }

    value = *eventFlag;
    *eventFlag = 0U;

    return value;
}


uint8_t OneButton_WasPressed(OneButton_t *button)
{
    if (button == NULL)
    {
        return 0U;
    }

    return OneButton_ConsumeEvent(
        &button->pressedEvent);
}


uint8_t OneButton_WasReleased(OneButton_t *button)
{
    if (button == NULL)
    {
        return 0U;
    }

    return OneButton_ConsumeEvent(
        &button->releasedEvent);
}


uint8_t OneButton_WasClicked(OneButton_t *button)
{
    if (button == NULL)
    {
        return 0U;
    }

    return OneButton_ConsumeEvent(
        &button->clickEvent);
}


uint8_t OneButton_WasDoubleClicked(OneButton_t *button)
{
    if (button == NULL)
    {
        return 0U;
    }

    return OneButton_ConsumeEvent(
        &button->doubleClickEvent);
}


uint8_t OneButton_WasLongPressed(OneButton_t *button)
{
    if (button == NULL)
    {
        return 0U;
    }

    return OneButton_ConsumeEvent(
        &button->longPressEvent);
}
