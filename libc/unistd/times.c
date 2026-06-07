#include <sys/times.h>

clock_t times(struct tms *buf)
{
	int ret;
	asm volatile("int $0x80"
		: "=a"(ret)
		: "0"(43), "b"(buf)
		: "ecx", "edx");
	return (clock_t)ret;
}
