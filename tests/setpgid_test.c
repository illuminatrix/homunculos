#include <stdio.h>
#include <unistd.h>

int main(void)
{
	int pid = getpid();
	int ret;
	int failed = 0;

	/* Test 1: setpgid(0, 0) sets pgid to own pid — should return 0 */
	ret = setpgid(0, 0);
	if (ret == 0) {
		printf("PASS: setpgid(0,0) returned 0\n");
	} else {
		printf("FAIL: setpgid(0,0) returned %d, expected 0\n", ret);
		failed++;
	}

	/* Test 2: setpgid(0, 42) sets pgid to arbitrary value — should return 0 */
	ret = setpgid(0, 42);
	if (ret == 0) {
		printf("PASS: setpgid(0,42) returned 0\n");
	} else {
		printf("FAIL: setpgid(0,42) returned %d, expected 0\n", ret);
		failed++;
	}

	/* Test 3: setpgid(0, 0) resets pgid to own pid — should return 0 */
	ret = setpgid(0, 0);
	if (ret == 0) {
		printf("PASS: setpgid(0,0) second call returned 0\n");
	} else {
		printf("FAIL: setpgid(0,0) second call returned %d, expected 0\n", ret);
		failed++;
	}

	/* Test 4: setpgid(-1, 0) invalid pid — should return -1 */
	ret = setpgid(-1, 0);
	if (ret == -1) {
		printf("PASS: setpgid(-1,0) returned -1\n");
	} else {
		printf("FAIL: setpgid(-1,0) returned %d, expected -1\n", ret);
		failed++;
	}

	/* Test 5: setpgid(99999, 0) nonexistent pid — should return -1 */
	ret = setpgid(99999, 0);
	if (ret == -1) {
		printf("PASS: setpgid(99999,0) returned -1\n");
	} else {
		printf("FAIL: setpgid(99999,0) returned %d, expected -1\n", ret);
		failed++;
	}

	/* Test 6: fork — parent sets child's pgid, child sets parent's pgid */
	{
		int child = fork();
		if (child == 0) {
			/* child: just exit */
			exit(0);
		} else if (child > 0) {
			/* parent: try to set child's pgid */
			ret = setpgid(child, 100);
			if (ret == 0) {
				printf("PASS: parent setpgid(child=%d,100) returned 0\n", child);
			} else {
				printf("FAIL: parent setpgid(child=%d,100) returned %d\n", child, ret);
				failed++;
			}

			/* parent: nonexistent child pid */
			ret = setpgid(child + 500, 100);
			if (ret == -1) {
				printf("PASS: parent setpgid(bad=%d,100) returned -1\n", child + 500);
			} else {
				printf("FAIL: parent setpgid(bad=%d,100) returned %d\n", child + 500, ret);
				failed++;
			}

			/* wait for child */
			waitpid(child, 0, 0);
		} else {
			printf("FAIL: fork returned %d\n", child);
			failed++;
		}
	}

	if (failed == 0)
		printf("ALL TESTS PASS\n");
	else
		printf("FAILED %d test(s)\n", failed);

	return 0;
}
