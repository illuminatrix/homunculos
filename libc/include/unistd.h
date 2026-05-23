#ifndef _UNISTD_H
#define _UNISTD_H 1

#include <sys/cdefs.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

int fork(void);
int read(int, void *, size_t);
int open(const char *, int);
int close(int);
int exec(const char *);
int waitpid(int, int *);
int mount(const char *, const char *, const char *);
int unmount(const char *);
int yield(void);
int reboot(void);
void exit(int);

#ifdef __cplusplus
}
#endif

#endif
