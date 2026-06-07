#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char *argv[])
{
	int fd, ret;

	if (argc != 3) {
		printf("usage: fchmod <path> <octal_mode>\n");
		return 1;
	}

	fd = open(argv[1], O_RDONLY);
	if (fd < 0) {
		printf("fchmod: open failed\n");
		return 1;
	}

	int mode = 0;
	char *end = argv[2];
	while (*end) {
		if (*end < '0' || *end > '7') {
			printf("fchmod: invalid mode\n");
			return 1;
		}
		mode = (mode << 3) | (*end - '0');
		end++;
	}

	ret = fchmod(fd, mode);
	close(fd);

	if (ret < 0) {
		printf("fchmod failed\n");
		return 1;
	}

	printf("fchmod ok\n");
	return 0;
}
