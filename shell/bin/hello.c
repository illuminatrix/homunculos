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

	printf("hello from exec\n");
	printf("argc=%d\n", argc);
	for (int i = 0; i < argc; i++)
		printf("argv[%d]=%s\n", i, argv[i]);
	exit(0);
}
