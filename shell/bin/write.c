#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

int main(int argc, char **argv)
{

	if (argc < 3) {
		printf("write: usage: write <path> <content...>\n");
		exit(1);
	}

	int fd = open(argv[1], O_WRONLY | O_CREAT, 0644);
	if (fd < 0) {
		printf("write: %s: open error\n", argv[1]);
		exit(1);
	}

	char buf[256];
	int pos = 0;
	for (int i = 2; i < argc; i++) {
		if (i > 2)
			buf[pos++] = ' ';
		const char *s = argv[i];
		while (*s && pos < 255)
			buf[pos++] = *s++;
	}

	int n = write(fd, buf, pos);
	if (n < 0) {
		printf("write: %s: write error\n", argv[1]);
		close(fd);
		exit(1);
	}
	close(fd);
	return 0;
}
