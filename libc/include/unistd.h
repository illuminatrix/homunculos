#ifndef _UNISTD_H
#define _UNISTD_H 1

#include <sys/cdefs.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

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
int getpid(void);
int getppid(void);
int lseek(int, int, int);
int brk(void *);
void *sbrk(int);
void exit(int);

#ifdef __cplusplus
}
#endif

#endif
