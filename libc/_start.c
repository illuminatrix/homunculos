#include <unistd.h>
#include <string.h>

int main(int argc, char **argv);

void __env_init(char **envp);

__attribute__((noreturn))
void _start(void)
{
	int argc;
	char **argv;
	char **envp;
	__asm__ volatile(
		"movl 4(%%ebp), %0\n\t"
		"leal 8(%%ebp), %1\n\t"
		: "=r"(argc), "=r"(argv)
		:
		: "memory"
	);
	envp = argv + argc + 1;
	__env_init(envp);
	exit(main(argc, argv));
}
