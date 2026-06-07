#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>

int main(void)
{
	printf("fcntl_test: hello world\n");

	int fd = open("/hello.txt", 0);
	if (fd < 0) {
		printf("fcntl_test: open FAIL: %d\n", fd);
		return 1;
	}
	printf("fcntl_test: fd=%d\n", fd);

	/* F_GETFD */
	int flags = fcntl(fd, F_GETFD, 0);
	if (flags < 0) {
		printf("fcntl_test: F_GETFD FAIL: %d\n", flags);
		return 1;
	}
	printf("fcntl_test: F_GETFD=%d\n", flags);

	/* F_SETFD with FD_CLOEXEC */
	int ret = fcntl(fd, F_SETFD, FD_CLOEXEC);
	if (ret < 0) {
		printf("fcntl_test: F_SETFD FAIL: %d\n", ret);
		return 1;
	}
	printf("fcntl_test: F_SETFD OK\n");

	/* Verify */
	flags = fcntl(fd, F_GETFD, 0);
	if (flags != FD_CLOEXEC) {
		printf("fcntl_test: verify F_GETFD FAIL: got %d, expected %d\n", flags, FD_CLOEXEC);
		return 1;
	}
	printf("fcntl_test: verify F_GETFD=%d OK\n", flags);

	/* F_GETFL */
	flags = fcntl(fd, F_GETFL, 0);
	if (flags < 0) {
		printf("fcntl_test: F_GETFL FAIL: %d\n", flags);
		return 1;
	}
	printf("fcntl_test: F_GETFL=%d\n", flags);

	/* F_DUPFD */
	int newfd = fcntl(fd, F_DUPFD, 0);
	if (newfd < 0) {
		printf("fcntl_test: F_DUPFD FAIL: %d\n", newfd);
		return 1;
	}
	printf("fcntl_test: F_DUPFD=%d\n", newfd);

	if (newfd == fd) {
		printf("fcntl_test: F_DUPFD got same fd (%d), not good\n", newfd);
		return 1;
	}

	close(newfd);
	close(fd);

	printf("fcntl_test: all OK\n");
	return 0;
}
