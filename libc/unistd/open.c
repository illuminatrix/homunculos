#include <unistd.h>
#include <fcntl.h>

int open(const char *path, int flags, ...)
{
	int mode = 0;
	if (flags & O_CREAT) {
		int *p = (int *)&flags;
		mode = *(++p);
	}
	long res;
	__asm__ volatile("int $0x80"
		: "=a"(res)
		: "0"(5),
		"b"((long)(path)),
		"c"((long)(flags)),
		"d"((long)(mode)));
	if (res >= 0)
		return (int)res;
	return -1;
}
