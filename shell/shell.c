#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "vfs.h"
#include "task.h"

#define SHELL_BUF_SIZE 64
#define SHELL_ARGS_MAX 4

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
			task_yield();
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

static void cmd_run(void)
{
	int pid = fork();
	if (pid < 0) {
		printf("exec: fork failed\n");
		return;
	}

	if (pid == 0) {
		char *argv[] = { "/bin/hello", 0 };
		if (execv("/bin/hello", argv) < 0)
			printf("exec: execv failed\n");
		exit(1);
	}

	int child_pid = waitpid(-1, 0);
	printf("exec: child %d done\n", child_pid);
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

static void shell_execute(const char *buf)
{
	if (strcmp(buf, "poweroff") == 0)
		reboot();
	else if (strcmp(buf, "greeting") == 0)
		printf("hello\n");
	else if (strncmp(buf, "run ", 4) == 0)
		cmd_run(buf + 4);
	else if (strcmp(buf, "run") == 0)
		cmd_run("/bin/shell");
	else if (strcmp(buf, "ls") == 0)
		cmd_ls("/");
	else if (strncmp(buf, "ls ", 3) == 0)
		cmd_ls(buf + 3);
	else if (strncmp(buf, "cat ", 4) == 0)
		cmd_cat(buf + 4);
	else if (buf[0] != '\0')
		printf("unknown\n");
}

void shell_main(void *arg)
{
	(void)arg;
	char buf[SHELL_BUF_SIZE];

	while (1) {
		shell_prompt();
		shell_readline(buf, SHELL_BUF_SIZE);
		shell_execute(buf);
	}
}
