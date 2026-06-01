#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>

int main(void)
{
	time_t t = time(0);
	printf("seconds=%d\n", (int)t);
	return 0;
}
