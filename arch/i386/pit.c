#include "pit.h"
#include "irq.h"
#include "pio.h"

static void (*pit_handler)(void) = 0;

static void pit_isr(void) {
	if (pit_handler)
		pit_handler();
}

void pit_init(uint32_t freq) {
	irq_request(0, pit_isr);

	uint32_t divisor = PIT_BASE_FREQ / freq;
	if (divisor > 0xFFFF)
		divisor = 0xFFFF;

	out(PIT_CMD_PORT, PIT_CMD_MODE_SQUARE);
	out(PIT_CHANNEL0, (uint8_t)(divisor & 0xFF));
	out(PIT_CHANNEL0, (uint8_t)(divisor >> 8));
}

void pit_set_callback(void (*callback)(void)) {
	pit_handler = callback;
}
