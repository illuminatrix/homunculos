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
		printf("rm: usage: rm <path>\n");
		exit(1);
	}

	if (unlink(argv[1]) < 0) {
		printf("rm: %s: error\n", argv[1]);
		exit(1);
	}
	exit(0);
}
