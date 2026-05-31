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
		printf("readlink: usage: readlink <path>\n");
		exit(1);
	}

	char buf[256];
	int n = readlink(argv[1], buf, sizeof(buf) - 1);
	if (n < 0) {
		printf("readlink: %s: error\n", argv[1]);
		exit(1);
	}
	buf[n] = '\0';
	printf("%s\n", buf);
	exit(0);
}
