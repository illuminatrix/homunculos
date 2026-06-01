#include <stdio.h>
#include <unistd.h>

int main(int argc, char **argv)
{

	if (argc < 2) {
		printf("readlink: usage: readlink <path>\n");
		exit(1);
	}

	char buf[256];
	int n = readlink(argv[1], buf, sizeof(buf) - 1);
	if (n < 0) {
		printf("readlink: %s: error\n", argv[1]);
		exit(1);
	}
	buf[n] = '\0';
	printf("%s\n", buf);
	return 0;
}
