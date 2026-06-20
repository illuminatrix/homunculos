#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>

int main(void)
{
	void *p;
	int page_size = 0x1000;
	int i, ok;
	char *buf;

	/* --- Test 1: mmap2 anonymous page, read/write --- */
	printf("Test 1: mmap2 anonymous...\n");
	p = mmap2(0, page_size, PROT_READ | PROT_WRITE,
		  MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
	if (p == MAP_FAILED) {
		printf("FAIL: mmap2 returned MAP_FAILED\n");
		return 1;
	}
	/* Write a pattern and verify */
	buf = (char *)p;
	for (i = 0; i < page_size; i++)
		buf[i] = (char)(i & 0xFF);
	ok = 1;
	for (i = 0; i < page_size; i++) {
		if (buf[i] != (char)(i & 0xFF)) {
			ok = 0;
			break;
		}
	}
	if (!ok) {
		printf("FAIL: mmap2 write/verify mismatch at byte %d\n", i);
		munmap(p, page_size);
		return 1;
	}
	printf("Test 1: PASS\n");

	/* --- Test 2: munmap then access should fault (skip, can't trap) --- */
	printf("Test 2: munmap...\n");
	if (munmap(p, page_size) < 0) {
		printf("FAIL: munmap returned error\n");
		return 1;
	}
	printf("Test 2: PASS\n");

	/* --- Test 3: mprotect read-only --- */
	printf("Test 3: mprotect PROT_READ...\n");
	p = mmap2(0, page_size, PROT_READ | PROT_WRITE,
		  MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
	if (p == MAP_FAILED) {
		printf("FAIL: mmap2 for test 3 returned MAP_FAILED\n");
		return 1;
	}
	/* write something */
	buf = (char *)p;
	buf[0] = 'A';
	/* mark read-only */
	if (mprotect(p, page_size, PROT_READ) < 0) {
		printf("FAIL: mprotect returned error\n");
		munmap(p, page_size);
		return 1;
	}
	/* verify we can still read */
	if (buf[0] != 'A') {
		printf("FAIL: data lost after mprotect\n");
		munmap(p, page_size);
		return 1;
	}
	printf("Test 3: PASS\n");
	munmap(p, page_size);

	/* --- Test 4: multiple mmap2 pages --- */
	printf("Test 4: multiple pages...\n");
	p = mmap2(0, page_size * 4, PROT_READ | PROT_WRITE,
		  MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
	if (p == MAP_FAILED) {
		printf("FAIL: mmap2 4 pages returned MAP_FAILED\n");
		return 1;
	}
	buf = (char *)p;
	for (i = 0; i < page_size * 4; i++)
		buf[i] = (char)(i & 0xFF);
	ok = 1;
	for (i = 0; i < page_size * 4; i++) {
		if (buf[i] != (char)(i & 0xFF)) {
			ok = 0;
			break;
		}
	}
	if (!ok) {
		printf("FAIL: multi-page write/verify mismatch at byte %d\n", i);
		munmap(p, page_size * 4);
		return 1;
	}
	printf("Test 4: PASS\n");
	munmap(p, page_size * 4);

	printf("\nALL TESTS PASS\n");
	return 0;
}
