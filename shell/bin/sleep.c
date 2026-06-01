#include <stdio.h>
#include <unistd.h>

int main(int argc, char **argv)
{

	if (argc < 2) {
		printf("usage: sleep <seconds>\n");
		exit(1);
	}

	int sec = 0;
	char *p = argv[1];
	while (*p) {
		if (*p < '0' || *p > '9') {
			printf("sleep: invalid number\n");
			exit(1);
		}
		sec = sec * 10 + (*p - '0');
		p++;
	}

	sleep((unsigned int)sec);
	return 0;
}
