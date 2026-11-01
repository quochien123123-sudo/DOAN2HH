#ifndef PUMP_WIFI_BUTTON_H
#define PUMP_WIFI_BUTTON_H
#include <stdint.h>

/* Portable active-pressed input state machine. Caller polls raw GPIO level
 * every ~10 ms. Debounce is 40 ms; a stable press emits once at 5000 ms after
 * the accepted raw falling edge. Release must debounce before rearming.
 * Unsigned subtraction keeps both intervals correct across millis() wrap. */
class WifiPortalButton {
public:
    explicit WifiPortalButton(uint32_t holdMs = 5000U, uint32_t debounceMs = 40U)
        : holdMs_(holdMs), debounceMs_(debounceMs) {}
    bool update(uint32_t now, bool pressed)
    {
        if (!initialized_) {
            initialized_ = true;
            rawPressed_ = pressed;
            rawChangedAt_ = now;
        } else if (pressed != rawPressed_) {
            rawPressed_ = pressed;
            rawChangedAt_ = now;
        }
        if (rawPressed_ != stablePressed_ && (uint32_t)(now - rawChangedAt_) >= debounceMs_) {
            stablePressed_ = rawPressed_;
            if (stablePressed_) pressedAt_ = rawChangedAt_;
            else fired_ = false;
        }
        if (stablePressed_ && rawPressed_ && !fired_ && (uint32_t)(now - pressedAt_) >= holdMs_) {
            fired_ = true;
            return true;
        }
        return false;
    }
private:
    const uint32_t holdMs_, debounceMs_;
    uint32_t rawChangedAt_ = 0, pressedAt_ = 0;
    bool initialized_ = false, rawPressed_ = false, stablePressed_ = false, fired_ = false;
};
#endif
