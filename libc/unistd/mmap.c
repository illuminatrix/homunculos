#include <sys/mman.h>

void *mmap2(void *addr, size_t length, int prot, int flags, int fd,
	    unsigned int offset)
{
	long res;
	__asm__ volatile("push %%ebp\n\t"
			 "mov %7, %%ebp\n\t"
			 "int $0x80\n\t"
			 "pop %%ebp\n\t"
		: "=a"(res)
		: "0"(192),
		"b"((long)(addr)),
		"c"((long)(length)),
		"d"((long)(prot)),
		"S"((long)(flags)),
		"D"((long)(fd)),
		"m"((long)(offset))
		: "memory");
	if ((void *)res >= (void *)-4096)
		return MAP_FAILED;
	return (void *)res;
}
