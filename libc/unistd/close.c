#include <unistd.h>

int close(int fd)
{
	long res;
	__asm__ volatile("int $0x80"
		: "=a"(res)
		: "0"(8),
		"b"((long)(fd)));
	if (res >= 0)
		return (int)res;
	return -1;
}
