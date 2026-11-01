#include "wifi_button.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

int main()
{
    WifiPortalButton button;
    assert(!button.update(0, false));
    assert(!button.update(100, true));
    assert(!button.update(139, true));
    assert(!button.update(140, true));
    assert(!button.update(5099, true));
    assert(button.update(5100, true));
    assert(!button.update(5101, true));
    assert(!button.update(60000, true));
    puts("PASS 4999 ms does not open portal; 5000 ms opens exactly once while held");

    assert(!button.update(60010, false));
    assert(!button.update(60030, false));
    assert(!button.update(60035, true)); // release bounce shorter than debounce
    assert(!button.update(70000, true));
    assert(!button.update(70100, false));
    assert(!button.update(70140, false)); // stable release rearms
    assert(!button.update(70200, true));
    assert(!button.update(70240, true));
    assert(!button.update(75199, true));
    assert(button.update(75200, true));
    puts("PASS holding/release bounce cannot repeat; stable release permits next long press");

    WifiPortalButton shortPress;
    assert(!shortPress.update(0, false));
    assert(!shortPress.update(1000, true));
    assert(!shortPress.update(1040, true));
    assert(!shortPress.update(5900, false));
    assert(!shortPress.update(5940, false));
    assert(!shortPress.update(6000, true));
    assert(!shortPress.update(6040, true));
    assert(!shortPress.update(10999, true));
    assert(shortPress.update(11000, true));
    puts("PASS separate short presses do not accumulate toward the five-second hold");

    WifiPortalButton bouncing;
    assert(!bouncing.update(0, false));
    assert(!bouncing.update(100, true));
    assert(!bouncing.update(120, false));
    assert(!bouncing.update(140, true));
    assert(!bouncing.update(180, true));
    assert(!bouncing.update(5139, true));
    assert(bouncing.update(5140, true));
    puts("PASS press-edge bounce restarts debounce and hold timing");

    WifiPortalButton wrap;
    const uint32_t started = UINT32_MAX - 2000U;
    assert(!wrap.update(started, true));
    assert(!wrap.update(started + 40U, true));
    assert(!wrap.update(started + 4999U, true));
    assert(wrap.update(started + 5000U, true));
    assert(!wrap.update(started + 10000U, true));
    puts("PASS timing across millis() wrap");

    WifiPortalButton bootHeld;
    assert(!bootHeld.update(0, true));
    assert(!bootHeld.update(40, true));
    assert(!bootHeld.update(4999, true));
    assert(bootHeld.update(5000, true));
    puts("PASS button already held at boot uses the same five-second threshold");
}
