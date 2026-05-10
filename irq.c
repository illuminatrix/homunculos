#include "irq.h"
#include "pio.h"

#define PIC1_CMD 0x20
#define PIC2_CMD 0xA0
#define PIC_EOI  0x20

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

    if (irq_handlers[irq])
        irq_handlers[irq]();

    if (irq >= 8)
        out(PIC2_CMD, PIC_EOI);
    out(PIC1_CMD, PIC_EOI);
}
