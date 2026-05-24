#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/utsname.h>
#include <sys/stat.h>

#define SHELL_BUF_SIZE 64

static void shell_prompt(void)
{
	printf("> ");
}

static int shell_readline(char *buf, int size)
{
	int pos = 0;

	while (1) {
		char c;
		int n = read(0, &c, 1);
		if (n <= 0) {
			yield();
			continue;
		}

		if (c == '\n') {
			printf("\n");
			buf[pos] = '\0';
			return pos;
		} else if (c == '\b') {
			if (pos > 0) {
				pos--;
				printf("\b \b");
			}
		} else if (pos < size - 1) {
			buf[pos++] = c;
			printf("%c", c);
		}
	}
}

static void cmd_ls(const char *path)
{
	int fd = open(path, 0);
	if (fd < 0) {
		printf("open failed\n");
		return;
	}
	char buf[128];
	int n = read(fd, buf, sizeof(buf) - 1);
	if (n < 0)
		n = 0;
	buf[n] = '\0';
	printf("%s\n", buf);
	close(fd);
}

static void cmd_cat(const char *name)
{
	int fd = open(name, 0);
	if (fd < 0) {
		printf("open failed\n");
		return;
	}
	char buf[128];
	int n = read(fd, buf, sizeof(buf) - 1);
	if (n < 0)
		n = 0;
	buf[n] = '\0';
	printf("%s\n", buf);
	close(fd);
}

static void cmd_uname(void)
{
	struct utsname uts;
	if (uname(&uts) < 0) {
		printf("uname failed\n");
		return;
	}
	printf("%s %s %s %s %s\n",
	       uts.sysname, uts.nodename,
	       uts.release, uts.version, uts.machine);
}

static void cmd_test_lseek(void)
{
	int fd = open("/dev/hello", 0);
	if (fd < 0) {
		printf("open failed\n");
		return;
	}
	char buf1[16], buf2[16];
	int n1 = read(fd, buf1, 10);
	if (n1 < 0) n1 = 0;
	buf1[n1] = '\0';
	printf("first read: \"%s\" (%d)\n", buf1, n1);

	int ret = lseek(fd, 0, SEEK_SET);
	if (ret < 0) {
		printf("lseek failed\n");
		close(fd);
		return;
	}
	printf("lseek returned %d\n", ret);

	int n2 = read(fd, buf2, 10);
	if (n2 < 0) n2 = 0;
	buf2[n2] = '\0';
	printf("second read: \"%s\" (%d)\n", buf2, n2);

	if (n1 == n2 && memcmp(buf1, buf2, n1) == 0)
		printf("lseek ok\n");
	else
		printf("lseek mismatch\n");

	close(fd);
}

static void cmd_test_stat(const char *path)
{
	struct stat st;
	if (stat(path, &st) < 0) {
		printf("stat %s failed\n", path);
		return;
	}
	printf("ino=%d mode=%d size=%d\n",
	       (int)st.st_ino, (int)st.st_mode, (int)st.st_size);
}

static void shell_execute(const char *buf)
{
	if (strcmp(buf, "poweroff") == 0)
		reboot();
	else if (strcmp(buf, "greeting") == 0)
		printf("hello\n");
	else if (strcmp(buf, "uname") == 0)
		cmd_uname();
	else if (strcmp(buf, "lseektest") == 0)
		cmd_test_lseek();
	else if (strcmp(buf, "ls") == 0)
		cmd_ls("/");
	else if (strncmp(buf, "ls ", 3) == 0)
		cmd_ls(buf + 3);
	else if (strncmp(buf, "cat ", 4) == 0)
		cmd_cat(buf + 4);
	else if (strncmp(buf, "stat ", 5) == 0)
		cmd_test_stat(buf + 5);
	else if (buf[0] != '\0')
		printf("unknown\n");
}

void _start(void)
{
	char buf[SHELL_BUF_SIZE];

	while (1) {
		shell_prompt();
		shell_readline(buf, SHELL_BUF_SIZE);
		shell_execute(buf);
	}
}
