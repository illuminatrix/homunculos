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
#define SYS_chdir     12
#define SYS_lseek     19
#define SYS_getpid     20
#define SYS_mount      21
#define SYS_umount     22
#define SYS_sched_yield 24
#define SYS_access     33
#define SYS_mkdir      39
#define SYS_rmdir      40
#define SYS_pipe       42
#define SYS_brk        45
#define SYS_dup2       63
#define SYS_getppid    64
#define SYS_symlink    83
#define SYS_readlink   85
#define SYS_unlink     87
#define SYS_reboot     88
#define SYS_stat      106
#define SYS_lstat     107
#define SYS_fstat     108
#define SYS_uname    122
#define SYS_getdents  141
#define SYS_getcwd   183
#define SYS_time       13
#define SYS_ioctl     54
#define SYS_gettimeofday 78
#define SYS_nanosleep  162

/* Signal syscalls */
#define SYS_kill      37
#define SYS_signal    48
#define SYS_rt_sigaction   67
#define SYS_rt_sigreturn  173
#define SYS_rt_sigprocmask 126

#define SYS_exit_group 252

/* Reboot magic values (Linux-compatible) */
#define LINUX_REBOOT_MAGIC1  0xfee1dead
#define LINUX_REBOOT_MAGIC2  0x28121969
#define LINUX_REBOOT_MAGIC2A 0x05121995
#define LINUX_REBOOT_MAGIC2B 0x16041998
#define LINUX_REBOOT_MAGIC2C 0x20112000

#define LINUX_REBOOT_CMD_RESTART    0x01234567
#define LINUX_REBOOT_CMD_POWER_OFF  0x4321fedc
#define LINUX_REBOOT_CMD_RESTART2   0xa1b2c3d4
#define LINUX_REBOOT_CMD_HALT       0xcdef0123

#endif
