#include <stdio.h>
#include <unistd.h>
#include <sys/termios.h>

int main(void)
{
	struct termios t;
	int ret;

	ret = ioctl(1, TCGETS, &t);
	if (ret == 0)
		printf("ok\n");
	else
		printf("fail: TCGETS stdout=%d\n", ret);

	ret = ioctl(0, TCGETS, &t);
	if (ret == 0)
		printf("ok\n");
	else
		printf("fail: TCGETS stdin=%d\n", ret);

	ret = ioctl(1, TCSETS, &t);
	if (ret == 0)
		printf("ok\n");
	else
		printf("fail: TCSETS=%d\n", ret);

	return 0;
}
