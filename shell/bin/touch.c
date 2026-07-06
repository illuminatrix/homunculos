#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

int main(int argc, char **argv)
{

	if (argc < 2) {
		printf("touch: usage: touch <path>\n");
		exit(1);
	}

	int fd = open(argv[1], O_WRONLY | O_CREAT, 0644);
	if (fd < 0) {
		printf("touch: %s: error\n", argv[1]);
		exit(1);
	}
	close(fd);
	return 0;
}
