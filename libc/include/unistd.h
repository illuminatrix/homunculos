#ifndef _UNISTD_H
#define _UNISTD_H 1

#include <sys/cdefs.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

int fork(void);
int read(int, void *, size_t);
int exec(const void *);
int join(void);
int reboot(void);
void exit(int);

#ifdef __cplusplus
}
#endif

#endif
