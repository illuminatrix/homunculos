#include <stdio.h>
#include <unistd.h>
#include <dirent.h>

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

	const char *path = ".";
	if (argc > 1)
		path = argv[1];

	int fd = open(path, 0);
	if (fd < 0) {
		printf("open failed\n");
		exit(1);
	}

	char buf[256];
	int n = getdents(fd, (struct dirent *)buf, sizeof(buf));
	if (n < 0) {
		printf("getdents failed\n");
		close(fd);
		exit(1);
	}

	int pos = 0;
	while (pos < n) {
		struct dirent *d = (struct dirent *)(buf + pos);
		printf("%s ", d->d_name);
		pos += d->d_reclen;
	}
	printf("\n");

	close(fd);
	exit(0);
}
