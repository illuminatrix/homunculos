#include <stdio.h>
#include <unistd.h>
#include <string.h>

static int parse_mode(const char *s)
{
	int m = 0;
	while (*s) {
		if (*s < '0' || *s > '7')
			return -1;
		m = (m << 3) | (*s - '0');
		s++;
	}
	return m;
}

static void usage(void)
{
	printf("usage: umask [<mode>] [<cmd> <arg>...]\n");
}

int main(int argc, char **argv)
{
	if (argc == 1) {
		int old = umask(0);
		umask(old);
		printf("%o\n", old);
		return 0;
	}

	int mode = parse_mode(argv[1]);
	if (mode < 0) {
		usage();
		return 1;
	}

	if (argc == 2) {
		int old = umask(mode);
		printf("%o\n", old);
		return 0;
	}

	umask(mode);

	char path[64];
	const char *cmd = argv[2];
	if (cmd[0] != '/') {
		strcpy(path, "/bin/");
		strcpy(path + 5, cmd);
		cmd = path;
	}
	execv(cmd, argv + 2);
	printf("umask: exec failed\n");
	return 1;
}
