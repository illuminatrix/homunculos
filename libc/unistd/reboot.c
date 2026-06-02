#include <unistd.h>

int reboot(int magic1, int magic2, int cmd)
{
	long res;
	__asm__ volatile("int $0x80"
		: "=a"(res)
		: "0"(88),
		"b"((long)(magic1)),
		"c"((long)(magic2)),
		"d"((long)(cmd)));
	if (res >= 0)
		return (int)res;
	return -1;
}
