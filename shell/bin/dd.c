#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

static unsigned long parse_num(const char *s)
{
	unsigned long n = 0;
	while (*s) {
		if (*s >= '0' && *s <= '9')
			n = n * 10 + (*s - '0');
		else
			break;
		s++;
	}
	return n;
}

int main(int argc, char **argv)
{
	const char *ifile = 0;
	const char *ofile = 0;
	unsigned long bs = 512;
	unsigned long count = 0;

	for (int i = 1; i < argc; i++) {
		if (strncmp(argv[i], "if=", 3) == 0)
			ifile = argv[i] + 3;
		else if (strncmp(argv[i], "of=", 3) == 0)
			ofile = argv[i] + 3;
		else if (strncmp(argv[i], "bs=", 3) == 0)
			bs = parse_num(argv[i] + 3);
		else if (strncmp(argv[i], "count=", 6) == 0)
			count = parse_num(argv[i] + 6);
	}

	if (!ifile && !ofile) {
		printf("dd: usage: dd if=<file> of=<file> [bs=<size>] [count=<n>]\n");
		exit(1);
	}

	int src = -1;
	if (ifile) {
		src = open(ifile, O_RDONLY);
		if (src < 0) {
			printf("dd: %s: open error\n", ifile);
			exit(1);
		}
	}

	int dst = -1;
	if (ofile) {
		dst = open(ofile, O_WRONLY | O_CREAT | O_TRUNC);
		if (dst < 0) {
			printf("dd: %s: open error\n", ofile);
			if (src >= 0) close(src);
			exit(1);
		}
	}

	char buf[1024];
	unsigned long total = 0;
	unsigned long i = 0;
	int n;

	if (bs > sizeof(buf))
		bs = sizeof(buf);

	while (1) {
		if (count && i >= count)
			break;

		if (src >= 0) {
			n = read(src, buf, bs);
			if (n <= 0)
				break;
		} else {
			n = bs;
		}

		if (dst >= 0) {
			int w = write(dst, buf, n);
			if (w < 0)
				break;
		}

		total += n;
		i++;
	}

	printf("%lu+0 records in\n", total / bs);
	printf("%lu+0 records out\n", i);

	if (src >= 0) close(src);
	if (dst >= 0) close(dst);
	return 0;
}
