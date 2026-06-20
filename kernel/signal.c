#include "signal.h"
#include "task.h"
#include "scheduler.h"
#include <string.h>
#include <stddef.h>

extern struct task tasks[];
extern int next_pid;

/* Forward declarations */
static int signal_default_terminates(int sig);

void signal_init_task(struct task *t)
{
	int i;
	t->pending_signals = 0;
	t->signal_mask = 0;
	t->signal_sigframe = 0;
	for (i = 0; i < NSIG; i++) {
		t->signal_handlers[i] = SIG_DFL;
		t->signal_handlers_mask[i] = 0;
		t->signal_handlers_flags[i] = 0;
	}
}

void signal_send(int pid, int sig)
{
	if (sig < 1 || sig >= NSIG)
		return;

	for (int i = 0; i < next_pid; i++) {
		if (tasks[i].pid == pid
		    && tasks[i].state != TASK_STATE_EXITED) {
			tasks[i].pending_signals |= (1 << (sig - 1));
			if (tasks[i].state == TASK_STATE_BLOCKED)
				tasks[i].state = TASK_STATE_READY;
			return;
		}
	}
}

static int signal_default_terminates(int sig)
{
	switch (sig) {
	case SIGCHLD:
	case SIGCONT:
		return 0;
	default:
		return 1;
	}
}

int sys_kill(int pid, int sig)
{
	if (sig < 1 || sig >= NSIG)
		return -1;
	if (pid <= 0)
		return -1;

	signal_send(pid, sig);
	return 0;
}

int sys_signal(int signum, void (*handler)(int))
{
	struct task *current = scheduler_get_current();
	void (*old)(int);

	if (!current || signum < 1 || signum >= NSIG)
		return -1;
	if (signum == SIGKILL || signum == SIGSTOP)
		return -1;

	old = current->signal_handlers[signum];
	current->signal_handlers[signum] = handler;
	return (int)old;
}

int sys_rt_sigaction(int signum, const struct sigaction *act,
		     struct sigaction *oldact)
{
	struct task *current = scheduler_get_current();

	if (!current || signum < 1 || signum >= NSIG)
		return -1;
	if (signum == SIGKILL || signum == SIGSTOP)
		return -1;

	if (oldact) {
		oldact->sa_handler = current->signal_handlers[signum];
		oldact->sa_mask = current->signal_handlers_mask[signum];
		oldact->sa_flags = current->signal_handlers_flags[signum];
	}

	if (act) {
		current->signal_handlers[signum] = act->sa_handler;
		current->signal_handlers_mask[signum] = act->sa_mask;
		current->signal_handlers_flags[signum] = act->sa_flags;
	}

	return 0;
}

int sys_rt_sigprocmask(int how, const uint32_t *set, uint32_t *oldset)
{
	struct task *current = scheduler_get_current();
	uint32_t new_mask;

	if (!current)
		return -1;

	if (oldset)
		*oldset = current->signal_mask;

	if (!set)
		return 0;

	new_mask = *set;

	switch (how) {
	case SIG_BLOCK:
		current->signal_mask |= new_mask;
		break;
	case SIG_UNBLOCK:
		current->signal_mask &= ~new_mask;
		break;
	case SIG_SETMASK:
		current->signal_mask = new_mask;
		break;
	default:
		return -1;
	}

	current->signal_mask &= ~((1 << (SIGKILL - 1))
				  | (1 << (SIGSTOP - 1)));
	return 0;
}

int sys_rt_sigreturn(void)
{
	struct task *current = scheduler_get_current();
	struct sigframe frame;
	uint32_t old_cr3;
	uint32_t ebp;

	if (!current || !current->signal_sigframe)
		return -1;

	/* Read sigframe from user space (need task's pdir) */
	__asm__ volatile("mov %%cr3, %0" : "=r"(old_cr3));
	__asm__ volatile("mov %0, %%cr3" :: "r"(current->pdir));
	memcpy(&frame, (void *)current->signal_sigframe, sizeof(frame));
	__asm__ volatile("mov %0, %%cr3" :: "r"(old_cr3));

	current->signal_sigframe = 0;
	current->signal_mask = frame.saved_mask;

	/*
	 * Modify the iretl frame on the kernel stack so the syscall
	 * return goes to the saved context instead of the trampoline.
	 *
	 * Stack layout in sys_rt_sigreturn after C prologue (6-arg dispatch):
	 *   [ebp]      = old ebp
	 *   [ebp+4]    = return addr (to syscall_handler after call)
	 *   [ebp+8]    = ebx (arg1)
	 *   [ebp+12]   = ecx (arg2)
	 *   [ebp+16]   = edx (arg3)
	 *   [ebp+20]   = esi (arg4)
	 *   [ebp+24]   = edi (arg5)
	 *   [ebp+28]   = ebp (arg6)
	 *   [ebp+32]   = eip (from int $0x80)
	 *   [ebp+36]   = cs
	 *   [ebp+40]   = eflags
	 *   [ebp+44]   = user_esp
	 *   [ebp+48]   = ss
	 */
	__asm__ volatile("mov %%ebp, %0" : "=r"(ebp));

	uint32_t *iret = (uint32_t *)(ebp + 32);
	iret[0] = frame.saved_eip;
	iret[1] = frame.saved_cs;
	iret[2] = frame.saved_eflags;
	iret[3] = frame.saved_esp;
	iret[4] = frame.saved_ss;

	return 0;
}

void signal_check_and_deliver(struct iret_frame *frame)
{
	struct task *current = scheduler_get_current();
	uint32_t pending;
	int sig;

	if (!current || !current->is_user)
		return;

	/* Only deliver to user-mode tasks (cs == 0x1B) */
	if (frame->cs != 0x1B)
		return;

	pending = current->pending_signals & ~current->signal_mask;

	while (pending) {
		/* Find lowest-numbered pending signal */
		for (sig = 1; sig < NSIG; sig++) {
			if (pending & (1 << (sig - 1)))
				break;
		}
		if (sig >= NSIG)
			break;

		pending &= ~(1 << (sig - 1));
		current->pending_signals &= ~(1 << (sig - 1));

		if (sig == SIGKILL || sig == SIGSTOP) {
			task_set_exit_status(sig);
			task_exit();
			return;
		}

		void (*handler)(int) = current->signal_handlers[sig];
		if (handler == SIG_IGN) {
			continue;
		}

		if (handler == SIG_DFL) {
			if (!signal_default_terminates(sig))
				continue;
			task_set_exit_status(sig);
			task_exit();
			return;
		}

		/*
		 * User handler: push sigframe + handler call frame
		 * onto the user stack, then redirect the iret frame.
		 */
		uint32_t old_esp = frame->user_esp;
		uint32_t old_ss = frame->ss;
		uint32_t frame_esp;
		struct sigframe sf;
		uint32_t trampoline_addr;
		uint32_t new_esp;
		uint32_t old_cr3;

		/* Build sigframe in kernel stack first */
		sf.saved_eip = frame->eip;
		sf.saved_cs = frame->cs;
		sf.saved_eflags = frame->eflags;
		sf.saved_esp = old_esp;
		sf.saved_ss = old_ss;
		sf.saved_mask = current->signal_mask;

		sf.trampoline[0] = 0xB8;
		sf.trampoline[1] = 0xAD;
		sf.trampoline[2] = 0x00;
		sf.trampoline[3] = 0x00;
		sf.trampoline[4] = 0x00;
		sf.trampoline[5] = 0xCD;
		sf.trampoline[6] = 0x80;
		sf.trampoline[7] = 0x00;

		frame_esp = old_esp - sizeof(struct sigframe);
		trampoline_addr = frame_esp
			+ offsetof(struct sigframe, trampoline);

		/*
		 * Handler call frame on user stack:
		 *   [new_esp]     = trampoline_addr (return address)
		 *   [new_esp + 4] = sig (argument)
		 *   [new_esp + 8] = sigframe (32 bytes)
		 */
		new_esp = frame_esp - 8;

		/* Write to user space via task's page directory */
		__asm__ volatile("mov %%cr3, %0" : "=r"(old_cr3));
		__asm__ volatile("mov %0, %%cr3" :: "r"(current->pdir));

		/* Write handler call frame */
		((uint32_t *)new_esp)[0] = trampoline_addr;
		((uint32_t *)new_esp)[1] = sig;

		/* Write sigframe */
		memcpy((void *)frame_esp, &sf, sizeof(sf));

		__asm__ volatile("mov %0, %%cr3" :: "r"(old_cr3));

		/* Save sigframe location */
		current->signal_sigframe = frame_esp;

		/* Update signal mask (block this signal during handler) */
		if (!(current->signal_handlers_flags[sig] & SA_NODEFER))
			current->signal_mask |= (1 << (sig - 1));
		current->signal_mask |= current->signal_handlers_mask[sig];
		current->signal_mask &= ~((1 << (SIGKILL - 1))
					  | (1 << (SIGSTOP - 1)));

		/* Redirect iret frame to signal handler */
		frame->eip = (uint32_t)handler;
		frame->user_esp = new_esp;

		return;
	}
}
