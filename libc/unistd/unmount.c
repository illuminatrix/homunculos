#include <unistd.h>

int unmount(const char *target)
{
	long res;
	__asm__ volatile("int $0x80"
		: "=a"(res)
		: "0"(10),
		"b"((long)(target)));
	if (res >= 0)
		return (int)res;
	return -1;
}
