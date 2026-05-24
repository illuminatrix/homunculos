#include <unistd.h>
#include <stddef.h>

int getcwd(char *buf, size_t size)
{
	int ret;

	asm volatile(
		"int $0x80"
		: "=a"(ret)
		: "0"(183), "b"(buf), "c"(size)
		: "memory"
	);

	return ret;
}
