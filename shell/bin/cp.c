#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

int main(int argc, char **argv)
{
	if (argc < 3) {
		printf("cp: usage: cp <src> <dst>\n");
		exit(1);
	}

	int src = open(argv[1], O_RDONLY);
	if (src < 0) {
		printf("cp: %s: open error\n", argv[1]);
		exit(1);
	}

	int dst = open(argv[2], O_WRONLY | O_CREAT | O_TRUNC);
	if (dst < 0) {
		printf("cp: %s: open error\n", argv[2]);
		close(src);
		exit(1);
	}

	char buf[512];
	int n;
	while ((n = read(src, buf, sizeof(buf))) > 0) {
		int w = write(dst, buf, n);
		if (w < 0) {
			printf("cp: write error\n");
			break;
		}
	}

	close(src);
	close(dst);
	return 0;
}
