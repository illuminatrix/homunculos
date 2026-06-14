#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>

typedef unsigned int dev_t;

static int parse_num(const char *s)
{
	int n = 0;
	while (*s >= '0' && *s <= '9') {
		n = n * 10 + (*s - '0');
		s++;
	}
	return n;
}

static void usage(void)
{
	printf("usage: mknod <path> <b|c> <major> <minor>\n");
}

int main(int argc, char **argv)
{
	char *path;
	char type;
	int major, minor;
	int mode;

	if (argc != 5) {
		usage();
		return 1;
	}

	path = argv[1];
	type = argv[2][0];
	major = parse_num(argv[3]);
	minor = parse_num(argv[4]);

	if (type == 'b')
		mode = S_IFBLK | 0666;
	else if (type == 'c' || type == 'u')
		mode = S_IFCHR | 0666;
	else {
		printf("mknod: invalid type '%c' (use b or c)\n", type);
		return 1;
	}

	if (major > 255 || minor > 255) {
		printf("mknod: major/minor must be 0-255\n");
		return 1;
	}

	dev_t dev = ((dev_t)major << 8) | minor;

	if (mknod(path, mode, dev) < 0) {
		printf("mknod: failed\n");
		return 1;
	}

	return 0;
}
