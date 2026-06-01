#include <unistd.h>

int main(void)
{
	int pid = fork();
	if (pid == 0) {
		exec("/bin/shell");
		const char msg[] = "init: exec failed\n";
		asm volatile("int $0x80"
			: : "a"(4), "b"(2), "c"(msg), "d"(17));
		exit(1);
	} else if (pid > 0) {
		waitpid(pid, 0, 0);
		exit(0);
	}

	const char msg[] = "init: fork failed\n";
	asm volatile("int $0x80"
		: : "a"(4), "b"(2), "c"(msg), "d"(18));
	exit(1);
}
