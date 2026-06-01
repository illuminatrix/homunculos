#include <stdio.h>
#include <unistd.h>

int main(int argc, char **argv)
{

	if (argc < 2) {
		printf("rm: usage: rm <path>\n");
		exit(1);
	}

	if (unlink(argv[1]) < 0) {
		printf("rm: %s: error\n", argv[1]);
		exit(1);
	}
	return 0;
}
