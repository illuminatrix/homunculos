#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>

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
		printf("usage: stat <path>\n");
		exit(1);
	}

	struct stat st;
	if (stat(argv[1], &st) < 0) {
		printf("stat failed\n");
		exit(1);
	}
	printf("ino=%d mode=%d size=%d\n",
	       (int)st.st_ino, (int)st.st_mode, (int)st.st_size);
	exit(0);
}
