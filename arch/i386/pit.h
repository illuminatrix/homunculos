#ifndef PIT_H
#define PIT_H

#include <stdint.h>
#include "pio.h"

#define PIT_CHANNEL0 0x40
#define PIT_CHANNEL1 0x41
#define PIT_CHANNEL2 0x42
#define PIT_CMD_PORT 0x43

#define PIT_CMD_MODE_SQUARE 0x36
#define PIT_BASE_FREQ 1193182

void pit_init(uint32_t freq);
void pit_set_callback(void (*callback)(void));

#endif
