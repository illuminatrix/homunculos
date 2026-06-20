#ifndef _SYS_MMAN_H
#define _SYS_MMAN_H 1

#include <sys/cdefs.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PROT_READ     1
#define PROT_WRITE    2
#define PROT_NONE     0

#define MAP_SHARED    1
#define MAP_PRIVATE   2
#define MAP_ANONYMOUS 32
#define MAP_FIXED     16
#define MAP_FAILED    ((void *)-1)

void *mmap2(void *addr, size_t length, int prot, int flags, int fd, unsigned int offset);
int munmap(void *addr, size_t length);
int mprotect(void *addr, size_t length, int prot);

#ifdef __cplusplus
}
#endif

#endif
