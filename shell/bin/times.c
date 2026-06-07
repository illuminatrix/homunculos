#include <stdio.h>
#include <sys/times.h>

int main(void)
{
	struct tms t;
	clock_t ticks = times(&t);
	printf("ticks=%d utime=%d stime=%d\n",
	       (int)ticks, (int)t.tms_utime, (int)t.tms_stime);
	return 0;
}
