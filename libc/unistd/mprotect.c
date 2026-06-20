#include <sys/mman.h>

int mprotect(void *addr, size_t length, int prot)
{
	long res;
	__asm__ volatile("int $0x80"
		: "=a"(res)
		: "0"(125),
		"b"((long)(addr)),
		"c"((long)(length)),
		"d"((long)(prot)));
	if (res >= 0)
		return (int)res;
	return -1;
}
