#include <stdio.h>
#include <unistd.h>

int main(int argc, char **argv)
{

	if (argc < 2) {
		printf("mkdir: usage: mkdir <path>\n");
		exit(1);
	}

	if (mkdir(argv[1], 0777) < 0) {
		printf("mkdir: %s: error\n", argv[1]);
		exit(1);
	}
	return 0;
}
