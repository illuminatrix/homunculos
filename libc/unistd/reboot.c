#include <unistd.h>

int reboot(void)
{
	long res;
	__asm__ volatile("int $0x80"
		: "=a"(res)
		: "0"(88));
	return (int)res;
}
