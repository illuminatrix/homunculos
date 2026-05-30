#ifndef _SYS_IOCTL_H
#define _SYS_IOCTL_H 1

#ifdef __cplusplus
extern "C" {
#endif

int ioctl(int fd, int cmd, void *arg);

#ifdef __cplusplus
}
#endif

#endif
