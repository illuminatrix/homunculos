#include "irq.h"

irq_handler_t irq_handlers[32] = {0};

uint8_t irq_request(uint8_t irq, irq_handler_t handler)
{
    if (irq >= 32)
        return IRQ_REQUEST_ERROR_INVALID;

    irq_handlers[irq] = handler;
    return IRQ_REQUEST_ERROR_OK;
}

void irq_handler(uint8_t irq)
{
    if (irq >= 32)
        return;

    irq_handlers[irq]();
}
