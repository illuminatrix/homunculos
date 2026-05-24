#include <sys/stat.h>

int fstat(int fd, struct stat *buf)
{
	int ret;
	asm volatile("int $0x80"
		: "=a"(ret)
		: "0"(108), "b"(fd), "c"(buf)
		: "edx", "memory");
	return ret;
}
