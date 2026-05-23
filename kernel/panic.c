#include <stdio.h>
#include "panic.h"

void panic(const char *msg)
{
	printf("KERNEL PANIC: %s\n", msg);
	while (1)
		asm volatile("hlt");
}
