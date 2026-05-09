#include<stdio.h>
#include <stdint.h>

int
putchar(int ic)
{
    long res;
    __asm__ volatile ("int $0x80"
	: "=a" (res)
	: "0" (1),
	"b" ((long)(1)),
	"c" ((long)(&ic)),
	"d" ((long)(1)));
    if (res >= 0)
	return (int) res;
    return -1;
}

