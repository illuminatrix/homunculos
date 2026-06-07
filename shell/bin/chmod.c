#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>

static void usage(void)
{
	printf("usage: chmod <mode> <path>\n");
}

int main(int argc, char **argv)
{
	int mode;
	char *end;

	if (argc < 3) {
		usage();
		return 1;
	}

	mode = 0;
	end = argv[1];
	while (*end) {
		if (*end < '0' || *end > '7') {
			usage();
			return 1;
		}
		mode = (mode << 3) | (*end - '0');
		end++;
	}

	if (chmod(argv[2], mode) < 0) {
		printf("chmod failed\n");
		return 1;
	}

	return 0;
}
