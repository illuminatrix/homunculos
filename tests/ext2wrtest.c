#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

static int failures;

static void check(int cond, const char *msg)
{
	if (cond) {
		printf("PASS: %s\n", msg);
	} else {
		printf("FAIL: %s\n", msg);
		failures++;
	}
}

void _start(void)
{
	char buf[256];
	int fd, n;

	failures = 0;

	/* 1. Create a file and write to it */
	fd = open("/ext2wrtest.txt", O_CREAT | O_WRONLY);
	check(fd >= 0, "open(O_CREAT|O_WRONLY)");
	if (fd >= 0) {
		n = write(fd, "Hello from write!", 17);
		check(n == 17, "write 17 bytes");
		close(fd);
	}

	/* 2. Read it back */
	fd = open("/ext2wrtest.txt", O_RDONLY);
	check(fd >= 0, "open(O_RDONLY) after write");
	if (fd >= 0) {
		memset(buf, 0, sizeof(buf));
		n = read(fd, buf, sizeof(buf) - 1);
		check(n == 17 && strcmp(buf, "Hello from write!") == 0,
		     "read back 'Hello from write!'");
		close(fd);
	}

	/* 3. mkdir */
	check(mkdir("/ext2wrdir") == 0, "mkdir /ext2wrdir");

	/* 4. Create a nested file */
	fd = open("/ext2wrdir/nested.txt", O_CREAT | O_WRONLY);
	check(fd >= 0, "open nested file O_CREAT|O_WRONLY");
	if (fd >= 0) {
		n = write(fd, "nested", 6);
		check(n == 6, "write 'nested'");
		close(fd);

		fd = open("/ext2wrdir/nested.txt", O_RDONLY);
		memset(buf, 0, sizeof(buf));
		n = read(fd, buf, sizeof(buf) - 1);
		check(n == 6 && strcmp(buf, "nested") == 0,
		     "read back 'nested'");
		close(fd);
	}

	/* 5. Symlink */
	check(symlink("/ext2wrtest.txt", "/ext2wrtlink") == 0,
	      "symlink /ext2wrtlink -> /ext2wrtest.txt");

	/* 6. Readlink */
	memset(buf, 0, sizeof(buf));
	n = readlink("/ext2wrtlink", buf, sizeof(buf) - 1);
	check(n > 0 && strcmp(buf, "/ext2wrtest.txt") == 0,
	      "readlink -> '/ext2wrtest.txt'");

	/* 7. Unlink */
	check(unlink("/ext2wrtest.txt") == 0,
	      "unlink /ext2wrtest.txt");

	/* Verify unlinked file cannot be opened */
	fd = open("/ext2wrtest.txt", O_RDONLY);
	check(fd < 0, "open unlinked file fails");

	if (failures)
		printf("RESULT: %d test(s) FAILED\n", failures);
	else
		printf("RESULT: All tests PASSED\n");

	exit(0);
}
