#include <unistd.h>

int fork(void)
{
	int pid;
	__asm__ volatile("int $0x80"
		: "=a"(pid)
		: "0"(2)
		: "ebx", "ecx", "edx");
	return pid;
}
