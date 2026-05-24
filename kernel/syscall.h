#ifndef SYSCALL_H
#define SYSCALL_H

/* Linux i386 syscall numbers for compatibility */
#define SYS_exit        1
#define SYS_fork        2
#define SYS_read        3
#define SYS_write       4
#define SYS_open        5
#define SYS_close       6
#define SYS_waitpid     7
#define SYS_execve     11
#define SYS_lseek     19
#define SYS_getpid     20
#define SYS_mount      21
#define SYS_umount     22
#define SYS_sched_yield 24
#define SYS_brk        45
#define SYS_getppid    64
#define SYS_reboot     88
#define SYS_uname    122

#endif
