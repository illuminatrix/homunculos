#include <stdio.h>
#include <unistd.h>

static int my_setsid(void)
{
	int ret;
	asm volatile("int $0x80"
		: "=a"(ret)
		: "0"(66)
		: "memory");
	return ret;
}

int main(void)
{
	int pid = getpid();
	int sid = my_setsid();

	printf("pid=%d sid=%d\n", pid, sid);

	if (sid != pid) {
		printf("FAIL: setsid returned %d, expected %d (pid)\n", sid, pid);
		return 1;
	}

	printf("PASS: setsid returned pid (%d)\n", pid);

	int sid2 = my_setsid();
	printf("sid2=%d pid=%d\n", sid2, pid);
	if (sid2 != pid) {
		printf("FAIL: second setsid returned %d, expected %d\n", sid2, pid);
		return 1;
	}

	printf("PASS: second setsid also returned pid (%d)\n", pid);
	printf("ALL TESTS PASS\n");
	return 0;
}
