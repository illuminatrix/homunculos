#include <unistd.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
	int newfd;
	char buf[32];

	/* dup fd 1 (stdout) to a new fd */
	newfd = dup(1);
	if (newfd < 0) {
		printf("dup FAIL: returned %d\n", newfd);
		return 1;
	}

	/* Write to the new fd — should appear on stdout */
	const char *msg = "dup works\n";
	int written = write(newfd, msg, strlen(msg));
	if (written < 0) {
		printf("dup FAIL: write to dup'd fd failed\n");
		return 1;
	}

	close(newfd);
	printf("dup test passed\n");
	return 0;
}
