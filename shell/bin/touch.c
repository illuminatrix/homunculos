#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

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
		printf("touch: usage: touch <path>\n");
		exit(1);
	}

	int fd = open(argv[1], O_WRONLY | O_CREAT);
	if (fd < 0) {
		printf("touch: %s: error\n", argv[1]);
		exit(1);
	}
	close(fd);
	exit(0);
}
