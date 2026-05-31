#include <sys/time.h>

int nanosleep(const struct timespec *req, struct timespec *rem)
{
	int ret;
	asm volatile("int $0x80"
		: "=a"(ret)
		: "0"(162), "b"(req), "c"(rem)
		: "edx", "memory");
	return ret;
}
