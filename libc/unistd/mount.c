#include <unistd.h>

int mount(const char *source, const char *target, const char *fstype)
{
	long res;
	__asm__ volatile("int $0x80"
		: "=a"(res)
		: "0"(9),
		"b"((long)(source)),
		"c"((long)(target)),
		"d"((long)(fstype)));
	if (res >= 0)
		return (int)res;
	return -1;
}
