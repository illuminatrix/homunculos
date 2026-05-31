#include <stdio.h>
#include <unistd.h>
#include <string.h>

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

	if (argc < 3) {
		printf("ln: usage: ln [-s] <target> <linkname>\n");
		exit(1);
	}

	int sym = 0;
	const char *target;
	const char *linkname;

	if (strcmp(argv[1], "-s") == 0) {
		sym = 1;
		target = argv[2];
		linkname = argv[3];
	} else {
		target = argv[1];
		linkname = argv[2];
	}

	if (sym) {
		if (symlink(target, linkname) < 0) {
			printf("ln: %s: error\n", linkname);
			exit(1);
		}
	} else {
		printf("ln: hard link not supported\n");
		exit(1);
	}
	exit(0);
}
