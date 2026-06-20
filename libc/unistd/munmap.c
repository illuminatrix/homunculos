#include <sys/mman.h>

int munmap(void *addr, size_t length)
{
	long res;
	__asm__ volatile("int $0x80"
		: "=a"(res)
		: "0"(91),
		"b"((long)(addr)),
		"c"((long)(length)));
	if (res >= 0)
		return (int)res;
	return -1;
}
