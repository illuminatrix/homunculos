#include <stdio.h>
#include <unistd.h>

void _start(void)
{
	int argc;
	char **argv;
	asm volatile(
		"movl 4(%%ebp), %0\n\t"
		"leal 8(%%ebp), %1\n\t"
		: "=r"(argc), "=r"(argv)
		:
		: "memory"
	);

	if (argc < 2) {
		printf("mkdir: usage: mkdir <path>\n");
		exit(1);
	}

	if (mkdir(argv[1]) < 0) {
		printf("mkdir: %s: error\n", argv[1]);
		exit(1);
	}
	exit(0);
}
