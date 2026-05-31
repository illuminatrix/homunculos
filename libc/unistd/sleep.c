#include <sys/time.h>

unsigned int sleep(unsigned int seconds)
{
	struct timespec ts;
	ts.tv_sec = (time_t)seconds;
	ts.tv_nsec = 0;
	if (nanosleep(&ts, &ts) < 0)
		return ts.tv_sec + (ts.tv_nsec > 0 ? 1 : 0);
	return 0;
}
