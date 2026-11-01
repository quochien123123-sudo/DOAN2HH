#ifndef BOOT_COUNTER_H
#define BOOT_COUNTER_H
#include <stdint.h>
/* Journals in final 2KiB of STM32F103C8 flash. Linker FLASH MUST be 62K.
 * Call once after HAL_Init/clock, before starting radio. Zero = flash failure.
 * Counts normal resets without storing pump state. Mass erase clears journal. */
uint32_t BootCounter_Next(void);
#endif
