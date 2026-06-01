#include <stdio.h>
#include <unistd.h>
#include <string.h>

int main(int argc, char **argv)
{

	if (argc < 3) {
		printf("ln: usage: ln [-s] <target> <linkname>\n");
		exit(1);
	}

	int sym = 0;
	const char *target;
	const char *linkname;

	if (strcmp(argv[1], "-s") == 0) {
		sym = 1;
		target = argv[2];
		linkname = argv[3];
	} else {
		target = argv[1];
		linkname = argv[2];
	}

	if (sym) {
		if (symlink(target, linkname) < 0) {
			printf("ln: %s: error\n", linkname);
			exit(1);
		}
	} else {
		printf("ln: hard link not supported\n");
		exit(1);
	}
	return 0;
}
