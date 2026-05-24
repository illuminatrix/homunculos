#ifndef _DIRENT_H
#define _DIRENT_H 1

#include <sys/cdefs.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DT_UNKNOWN 0
#define DT_DIR     4
#define DT_REG     8

struct dirent {
	unsigned long  d_ino;
	unsigned long  d_off;
	unsigned short d_reclen;
	char           d_name[];
};

int getdents(int fd, struct dirent *dirp, int count);

#ifdef __cplusplus
}
#endif

#endif
