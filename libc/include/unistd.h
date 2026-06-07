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
int execv(const char *, char *const *);
int execve(const char *, char *const *, char *const *);
#define WNOHANG 1
int waitpid(int, int *, int);
int mount(const char *, const char *, const char *);
int unmount(const char *);
int yield(void);
int reboot(int, int, int);
int getpid(void);
int getppid(void);
int lseek(int, int, int);
int brk(void *);
void *sbrk(int);
__attribute__((noreturn)) void exit(int);
int chdir(const char *);
int getcwd(char *, size_t);
int ioctl(int, int, void *);
int pipe(int[2]);
int dup2(int, int);
int write(int, const void *, unsigned int);
int access(const char *);
int mkdir(const char *);
int rmdir(const char *);
int unlink(const char *);
int symlink(const char *, const char *);
int readlink(const char *, char *, unsigned int);
void sync(void);
int dup(int);
int chmod(const char *, int);
int rename(const char *, const char *);

/* Environment */
extern char **environ;

/* Time */
unsigned int sleep(unsigned int seconds);

#ifdef __cplusplus
}
#endif

#endif
