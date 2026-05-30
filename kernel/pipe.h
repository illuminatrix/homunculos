#ifndef PIPE_H
#define PIPE_H

#include <stdint.h>

int sys_pipe(int *fds);
int sys_dup2(int oldfd, int newfd);

#endif
