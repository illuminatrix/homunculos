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

	char buf[128];
	int n;

	if (argc < 2) {
		/* Read from stdin */
		n = read(0, buf, sizeof(buf) - 1);
		if (n < 0)
			n = 0;
		buf[n] = '\0';
		printf("%s\n", buf);
		exit(0);
	}

	int fd = open(argv[1], 0);
	if (fd < 0) {
		printf("open failed\n");
		exit(1);
	}
	n = read(fd, buf, sizeof(buf) - 1);
	if (n < 0)
		n = 0;
	buf[n] = '\0';
	printf("%s\n", buf);
	close(fd);
	exit(0);
}
