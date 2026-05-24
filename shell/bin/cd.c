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

	const char *path = "/";
	if (argc > 1)
		path = argv[1];

	if (chdir(path) < 0) {
		printf("cd: %s: failed\n", path);
		exit(1);
	}

	char buf[256];
	if (getcwd(buf, sizeof(buf)) < 0) {
		printf("cd: getcwd failed\n");
		exit(1);
	}
	printf("%s\n", buf);
	exit(0);
}
