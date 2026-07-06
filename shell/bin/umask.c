#include <stdio.h>
#include <unistd.h>

static void usage(void)
{
	printf("usage: umask [<mode>]\n");
}

int main(int argc, char **argv)
{
	int old;

	if (argc > 2) {
		usage();
		return 1;
	}

	if (argc == 1) {
		old = umask(0);
		umask(old);
		printf("%o\n", old);
		return 0;
	}

	char *end = argv[1];
	int mode = 0;
	while (*end) {
		if (*end < '0' || *end > '7') {
			usage();
			return 1;
		}
		mode = (mode << 3) | (*end - '0');
		end++;
	}

	old = umask(mode);
	printf("%o\n", old);
	return 0;
}
