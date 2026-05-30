#include <stdio.h>
#include <unistd.h>

void _start(void)
{
	char buf[128];
	int n = read(0, buf, sizeof(buf) - 1);
	if (n < 0)
		n = 0;
	buf[n] = '\0';
	printf("PIPE:%s\n", buf);
	exit(0);
}
