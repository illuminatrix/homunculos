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
		printf("usage: sleep <seconds>\n");
		exit(1);
	}

	int sec = 0;
	char *p = argv[1];
	while (*p) {
		if (*p < '0' || *p > '9') {
			printf("sleep: invalid number\n");
			exit(1);
		}
		sec = sec * 10 + (*p - '0');
		p++;
	}

	sleep((unsigned int)sec);
	exit(0);
}
