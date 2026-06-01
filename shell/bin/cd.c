#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
	const char *path = "/";
	if (argc > 1)
		path = argv[1];

	if (chdir(path) < 0) {
		printf("cd: %s: failed\n", path);
		exit(1);
	}

	char buf[256];
	if (getcwd(buf, sizeof(buf)) < 0) {
		printf("cd: getcwd failed\n");
		exit(1);
	}
	setenv("PWD", buf, 1);
	printf("%s\n", buf);
	return 0;
}
