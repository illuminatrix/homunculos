#ifndef SIGNAL_H
#define SIGNAL_H

#include <stdint.h>

struct task;

#define NSIG 32

/* Signal numbers (Linux/i386 compatible) */
#define SIGHUP     1
#define SIGINT     2
#define SIGQUIT    3
#define SIGILL     4
#define SIGTRAP    5
#define SIGABRT    6
#define SIGBUS     7
#define SIGFPE     8
#define SIGKILL    9
#define SIGUSR1   10
#define SIGSEGV   11
#define SIGUSR2   12
#define SIGPIPE   13
#define SIGALRM   14
#define SIGTERM   15
#define SIGCHLD   17
#define SIGCONT   18
#define SIGSTOP   19
#define SIGTSTP   20

#define SIG_DFL ((void (*)(int))0)
#define SIG_IGN ((void (*)(int))1)

#define SA_NODEFER 0x40000000

#define SIG_BLOCK   0
#define SIG_UNBLOCK 1
#define SIG_SETMASK 2

struct sigaction {
	void (*sa_handler)(int);
	uint32_t sa_mask;
	int sa_flags;
};

/* Signal frame on user stack */
#define SIGFRAME_TRAMP_SIZE 8

struct sigframe {
	uint32_t saved_eip;
	uint32_t saved_cs;
	uint32_t saved_eflags;
	uint32_t saved_esp;
	uint32_t saved_ss;
	uint32_t saved_mask;
	uint8_t trampoline[SIGFRAME_TRAMP_SIZE];
};

/* IRET frame on kernel stack (what iretl pops) */
struct iret_frame {
	uint32_t eip;
	uint32_t cs;
	uint32_t eflags;
	uint32_t user_esp;
	uint32_t ss;
};

/* Syscall implementations */
int sys_kill(int pid, int sig);
int sys_signal(int signum, void (*handler)(int));
int sys_rt_sigaction(int signum, const struct sigaction *act,
		     struct sigaction *oldact);
int sys_rt_sigprocmask(int how, const uint32_t *set, uint32_t *oldset);
int sys_rt_sigreturn(void);

/* Check pending signals and deliver if any. Modifies iret_frame. */
void signal_check_and_deliver(struct iret_frame *frame);

/* Initialize signal state for a new task */
void signal_init_task(struct task *t);

/* Send a signal to a specific task */
void signal_send(int pid, int sig);

#endif
