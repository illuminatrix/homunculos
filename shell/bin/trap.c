#include <stdio.h>
#include <unistd.h>

static volatile int signal_caught = 0;

static void handler(int sig)
{
	signal_caught = 1;
}

static int my_signal(int sig, void (*h)(int))
{
	int old;
	asm volatile("int $0x80"
		: "=a"(old)
		: "0"(48), "b"(sig), "c"(h)
		: "memory");
	return old;
}

static int read_num(char *p)
{
	int n = 0;
	while (*p) {
		if (*p < '0' || *p > '9')
			return -1;
		n = n * 10 + (*p - '0');
		p++;
	}
	return n;
}

static void itoa(int n, char *buf)
{
	int tmp = n, len = 0;
	if (n == 0) {
		buf[0] = '0';
		buf[1] = '\0';
		return;
	}
	do {
		tmp /= 10;
		len++;
	} while (tmp > 0);
	buf[len] = '\0';
	tmp = n;
	for (int i = len - 1; i >= 0; i--) {
		buf[i] = '0' + (tmp % 10);
		tmp /= 10;
	}
}

int main(int argc, char **argv)
{
	int signum;

	if (argc < 2) {
		printf("usage: trap <signum> [kill]\n");
		return 1;
	}

	signum = read_num(argv[1]);
	if (signum < 1 || signum > 31) {
		printf("trap: invalid signum\n");
		return 1;
	}

	my_signal(signum, handler);

	if (argc > 2) {
		/* Self-test: fork child that kills the parent */
		int pid = fork();
		if (pid < 0) {
			printf("trap: fork failed\n");
			return 1;
		}

		if (pid == 0) {
			char ppid_str[16], sig_str[16];
			sleep(1);
			itoa(getppid(), ppid_str);
			itoa(signum, sig_str);
			char *args[] = {"kill", ppid_str, sig_str, 0};
			execv("/bin/kill", args);
			printf("trap: kill not found\n");
			exit(1);
		}
	}

	printf("trap: waiting for signal %d\n", signum);
	while (!signal_caught)
		sleep(1);
	printf("CAUGHT\n");

	return 0;
}
