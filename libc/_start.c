#include <unistd.h>

int main(int argc, char **argv);

__attribute__((noreturn))
void _start(void)
{
	int argc;
	char **argv;
	__asm__ volatile(
		"movl 4(%%ebp), %0\n\t"
		"leal 8(%%ebp), %1\n\t"
		: "=r"(argc), "=r"(argv)
		:
		: "memory"
	);
	exit(main(argc, argv));
}
