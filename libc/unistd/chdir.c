#include <unistd.h>

int chdir(const char *path)
{
	int ret;

	asm volatile(
		"int $0x80"
		: "=a"(ret)
		: "0"(12), "b"(path)
	);

	return ret;
}
