#include <unistd.h>

int open(const char *path, int flags)
{
	long res;
	__asm__ volatile("int $0x80"
		: "=a"(res)
		: "0"(7),
		"b"((long)(path)),
		"c"((long)(flags)));
	if (res >= 0)
		return (int)res;
	return -1;
}
