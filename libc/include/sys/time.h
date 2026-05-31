#ifndef _SYS_TIME_H
#define _SYS_TIME_H 1

#ifdef __cplusplus
extern "C" {
#endif

typedef long time_t;

struct timeval {
	time_t tv_sec;
	long tv_usec;
};

struct timezone {
	int tz_minuteswest;
	int tz_dsttime;
};

struct timespec {
	time_t tv_sec;
	long tv_nsec;
};

int gettimeofday(struct timeval *tv, struct timezone *tz);
time_t time(time_t *tloc);
int nanosleep(const struct timespec *req, struct timespec *rem);
unsigned int sleep(unsigned int seconds);

#ifdef __cplusplus
}
#endif

#endif
