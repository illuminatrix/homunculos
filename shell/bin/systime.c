#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>

void _start(void)
{
	time_t t = time(0);
	printf("seconds=%d\n", (int)t);
	exit(0);
}
