#include <sys/stat.h>

int stat(const char *path, struct stat *buf)
{
	int ret;
	asm volatile("int $0x80"
		: "=a"(ret)
		: "0"(106), "b"(path), "c"(buf)
		: "edx", "memory");
	return ret;
}
