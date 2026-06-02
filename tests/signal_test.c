#include <stdio.h>
#include <unistd.h>

#define SIGUSR1 10
#define SIGTERM 15
#define SIGKILL 9
#define SIG_IGN ((void (*)(int))1)

static volatile int sigusr1_caught = 0;

static void sigusr1_handler(int sig)
{
	sigusr1_caught = 1;
}

static int my_signal(int sig, void (*handler)(int))
{
	int old;
	asm volatile("int $0x80"
		: "=a"(old)
		: "0"(48), "b"(sig), "c"(handler)
		: "memory");
	return old;
}

static int my_kill(int pid, int sig)
{
	int ret;
	asm volatile("int $0x80"
		: "=a"(ret)
		: "0"(37), "b"(pid), "c"(sig)
		: "memory");
	return ret;
}

int main(void)
{
	int pid = getpid();

	/* Test 1: SIGUSR1 handler delivery */
	{
		int old = my_signal(SIGUSR1, sigusr1_handler);
		if ((void *)old != (void *)0) {
			printf("FAIL: unexpected old handler\n");
			return 1;
		}

		int ret = my_kill(pid, SIGUSR1);
		if (ret != 0) {
			printf("FAIL: kill returned %d\n", ret);
			return 1;
		}

		if (sigusr1_caught)
			printf("PASS: SIGUSR1 handler called\n");
		else {
			printf("FAIL: SIGUSR1 handler not called\n");
			return 1;
		}
	}

	/* Test 2: SIGKILL cannot be caught */
	{
		int old = my_signal(SIGKILL, sigusr1_handler);
		if (old == -1)
			printf("PASS: SIGKILL cannot be caught\n");
		else {
			printf("FAIL: SIGKILL was unexpectedly catchable\n");
			return 1;
		}
	}

	/* Test 3: SIG_IGN for SIGTERM */
	{
		my_signal(SIGTERM, SIG_IGN);

		int ret = my_kill(pid, SIGTERM);
		if (ret != 0) {
			printf("FAIL: kill returned %d\n", ret);
			return 1;
		}

		printf("PASS: SIGTERM ignored\n");
	}

	printf("ALL TESTS PASS\n");
	return 0;
}
