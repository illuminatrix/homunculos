#include <stdio.h>
#include <unistd.h>

static void usage(void)
{
	printf("usage: mv <old> <new>\n");
}

int main(int argc, char **argv)
{
	if (argc < 3) {
		usage();
		return 1;
	}

	if (rename(argv[1], argv[2]) < 0) {
		printf("mv failed\n");
		return 1;
	}

	return 0;
}
