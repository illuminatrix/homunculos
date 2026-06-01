#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>

int main(int argc, char **argv)
{

	if (argc < 2) {
		printf("usage: stat <path>\n");
		exit(1);
	}

	struct stat st;
	if (stat(argv[1], &st) < 0) {
		printf("stat failed\n");
		exit(1);
	}
	printf("ino=%d mode=%d size=%d\n",
	       (int)st.st_ino, (int)st.st_mode, (int)st.st_size);
	return 0;
}
