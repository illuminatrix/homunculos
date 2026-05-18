#include <stdint.h>

#include "pio.h"

uint8_t in(uint16_t port)
{
    uint8_t ret;
    asm volatile ("inb %1, %0" : "=a" (ret) : "dN" (port));
    return ret;
}

void out(uint16_t port, uint8_t val)
{
    asm volatile ("outb %0, %1" : : "a"(val), "dN"(port));
}

uint16_t inw(uint16_t port)
{
	uint16_t ret;
	asm volatile ("inw %1, %0" : "=a" (ret) : "dN" (port));
	return ret;
}

void outw(uint16_t port, uint16_t val)
{
	asm volatile ("outw %0, %1" : : "a"(val), "dN"(port));
}

void outl(uint16_t port, uint32_t val)
{
    asm volatile ("outl %0, %1" : : "a"(val), "dN"(port));
}
