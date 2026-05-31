#include <sys/time.h>

int gettimeofday(struct timeval *tv, struct timezone *tz)
{
	int ret;
	asm volatile("int $0x80"
		: "=a"(ret)
		: "0"(78), "b"(tv), "c"(tz)
		: "edx", "memory");
	return ret;
}
