#include <unistd.h>

int read(int fd, void *buf, size_t nbyte)
{
	long res;
	__asm__ volatile("int $0x80"
		: "=a"(res)
		: "0"(3),
		"b"((long)(fd)),
		"c"((long)(buf)),
		"d"((long)(nbyte)));
	if (res >= 0)
		return (int)res;
	return -1;
}
