#include <stdio.h>
#include <unistd.h>

void _start(void)
{
	char buf[256];
	if (getcwd(buf, sizeof(buf)) < 0) {
		printf("getcwd failed\n");
		exit(1);
	}
	printf("%s\n", buf);
	exit(0);
}
