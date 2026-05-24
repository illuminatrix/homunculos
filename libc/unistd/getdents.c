#include <unistd.h>
#include <dirent.h>

int getdents(int fd, struct dirent *dirp, int count)
{
	int ret;

	asm volatile(
		"int $0x80"
		: "=a"(ret)
		: "0"(141), "b"(fd), "c"(dirp), "d"(count)
		: "memory"
	);

	return ret;
}
