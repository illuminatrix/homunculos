#include <stdio.h>
#include <unistd.h>

int main(int argc, char **argv)
{

	char buf[128];
	int n;

	if (argc < 2) {
		/* Read from stdin */
		n = read(0, buf, sizeof(buf) - 1);
		if (n < 0)
			n = 0;
		buf[n] = '\0';
		printf("%s\n", buf);
		exit(0);
	}

	int fd = open(argv[1], 0);
	if (fd < 0) {
		printf("open failed\n");
		exit(1);
	}
	n = read(fd, buf, sizeof(buf) - 1);
	if (n < 0)
		n = 0;
	buf[n] = '\0';
	printf("%s\n", buf);
	close(fd);
	return 0;
}
