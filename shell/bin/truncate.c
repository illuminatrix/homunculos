#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
	if (argc < 3) {
		printf("truncate: usage: truncate <size> <path>\n");
		exit(1);
	}

	unsigned long size = 0;
	char *s = argv[1];
	while (*s) {
		if (*s < '0' || *s > '9') {
			printf("truncate: invalid size\n");
			exit(1);
		}
		size = size * 10 + (*s - '0');
		s++;
	}

	if (truncate64(argv[2], size) < 0) {
		printf("truncate: %s: error\n", argv[2]);
		exit(1);
	}
	return 0;
}
