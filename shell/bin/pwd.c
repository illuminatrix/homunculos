#include <stdio.h>
#include <unistd.h>

int main(void)
{
	char buf[256];
	if (getcwd(buf, sizeof(buf)) < 0) {
		printf("getcwd failed\n");
		exit(1);
	}
	printf("%s\n", buf);
	return 0;
}
