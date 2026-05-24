#include <stdio.h>
#include <unistd.h>
#include <string.h>

#define SHELL_BUF_SIZE 64
#define CMD_ARGS_MAX 8

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

static void shell_execute(char *buf)
{
	char *argv[CMD_ARGS_MAX];
	int argc = 0;

	char *p = buf;
	while (*p && argc < CMD_ARGS_MAX - 1) {
		while (*p == ' ') p++;
		if (!*p) break;
		argv[argc++] = p;
		while (*p && *p != ' ') p++;
		if (*p) *p++ = '\0';
	}
	argv[argc] = 0;

	if (argc == 0)
		return;

	if (strcmp(argv[0], "cd") == 0) {
		const char *dir = "/";
		if (argc > 1)
			dir = argv[1];
		if (chdir(dir) < 0)
			printf("cd: %s: failed\n", dir);
		return;
	}

	int pid = fork();
	if (pid < 0) {
		printf("fork failed\n");
		return;
	}

	if (pid == 0) {
		char path[SHELL_BUF_SIZE + 8];
		if (argv[0][0] == '/') {
			execv(argv[0], argv);
		} else {
			strcpy(path, "/bin/");
			strcpy(path + 5, argv[0]);
			execv(path, argv);
		}
		printf("%s: not found\n", argv[0]);
		exit(1);
	}

	waitpid(pid, 0);
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
