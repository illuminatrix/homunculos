#include <sys/time.h>

time_t time(time_t *tloc)
{
	int ret;
	asm volatile("int $0x80"
		: "=a"(ret)
		: "0"(13), "b"(tloc)
		: "ecx", "edx", "memory");
	return (time_t)ret;
}
