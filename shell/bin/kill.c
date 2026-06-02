#include <stdio.h>
#include <unistd.h>

int main(int argc, char **argv)
{
	int pid = 0, signum = 0;
	char *p;

	if (argc < 3) {
		printf("usage: kill <pid> <signum>\n");
		return 1;
	}

	p = argv[1];
	while (*p) {
		if (*p < '0' || *p > '9') {
			printf("kill: invalid pid\n");
			return 1;
		}
		pid = pid * 10 + (*p - '0');
		p++;
	}

	p = argv[2];
	while (*p) {
		if (*p < '0' || *p > '9') {
			printf("kill: invalid signum\n");
			return 1;
		}
		signum = signum * 10 + (*p - '0');
		p++;
	}

	int ret;
	asm volatile("int $0x80"
		: "=a"(ret)
		: "0"(37), "b"(pid), "c"(signum)
		: "memory");

	if (ret < 0) {
		printf("kill: failed (%d)\n", ret);
		return 1;
	}

	return 0;
}
