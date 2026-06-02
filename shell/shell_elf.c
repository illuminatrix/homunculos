#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

#define SHELL_BUF_SIZE 64
#define CMD_ARGS_MAX 8
#define CMD_SEGMENTS_MAX 8

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

static int parse_args(char *buf, char *argv[], int max_args)
{
	int argc = 0;
	char *p = buf;

	while (*p && argc < max_args - 1) {
		while (*p == ' ') p++;
		if (!*p) break;
		argv[argc++] = p;
		while (*p && *p != ' ') p++;
		if (*p) *p++ = '\0';
	}
	argv[argc] = 0;
	return argc;
}

static int exec_command(char *argv[])
{
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
	return -1;
}

static void shell_execute(char *buf)
{
	/* Check for pipe character */
	char *pipe_pos = 0;
	char *p;

	for (p = buf; *p; p++) {
		if (*p == '|') {
			pipe_pos = p;
			break;
		}
	}

	if (!pipe_pos) {
		/* Single command */
		char *argv[CMD_ARGS_MAX];
		int argc = parse_args(buf, argv, CMD_ARGS_MAX);

		if (argc == 0)
			return;

		/* Parse output redirection (> file) */
		char *redirect_out = 0;
		for (int i = 0; argv[i]; i++) {
			if (strcmp(argv[i], ">") == 0) {
				if (!argv[i + 1]) {
					printf("syntax error: expected filename after >\n");
					return;
				}
				redirect_out = argv[i + 1];
				int j = i;
				while (argv[j + 2]) {
					argv[j] = argv[j + 2];
					j++;
				}
				argv[j] = 0;
				break;
			}
		}

		if (strcmp(argv[0], "cd") == 0) {
			const char *dir = "/";
			if (argc > 1)
				dir = argv[1];
			if (chdir(dir) < 0) {
				printf("cd: %s: failed\n", dir);
				return;
			}
			char cwd[256];
			if (getcwd(cwd, sizeof(cwd)) >= 0)
				setenv("PWD", cwd, 1);
			return;
		}

		int pid = fork();
		if (pid < 0) {
			printf("fork failed\n");
			return;
		}

		if (pid == 0) {
			if (redirect_out) {
				int fd = open(redirect_out, O_WRONLY | O_CREAT | O_TRUNC);
				if (fd < 0) {
					printf("redirection failed: %s\n", redirect_out);
					exit(1);
				}
				dup2(fd, 1);
				close(fd);
			}
			exec_command(argv);
		}

		waitpid(pid, 0, 0);
		return;
	}

	/* Pipeline: split by | */
	char *segments[CMD_SEGMENTS_MAX];
	int nsegs = 0;
	char *cur = buf;

	segments[nsegs++] = cur;
	for (p = cur; *p; p++) {
		if (*p == '|') {
			*p = '\0';
			if (nsegs < CMD_SEGMENTS_MAX)
				segments[nsegs++] = p + 1;
		}
	}

	if (nsegs < 2) {
		printf("invalid pipe\n");
		return;
	}

	int prev_fd = -1;
	int pids[CMD_SEGMENTS_MAX];
	int i;

	for (i = 0; i < nsegs; i++) {
		char *argv[CMD_ARGS_MAX];
		int argc = parse_args(segments[i], argv, CMD_ARGS_MAX);

		if (argc == 0)
			continue;

		int pfd[2];
		if (i < nsegs - 1)
			pipe(pfd);

		int pid = fork();
		if (pid < 0) {
			printf("fork failed\n");
			return;
		}

		if (pid == 0) {
			if (prev_fd != -1) {
				dup2(prev_fd, 0);
				close(prev_fd);
			}
			if (i < nsegs - 1) {
				close(pfd[0]);
				dup2(pfd[1], 1);
				close(pfd[1]);
			}
			exec_command(argv);
		}

		if (prev_fd != -1)
			close(prev_fd);
		if (i < nsegs - 1) {
			close(pfd[1]);
			prev_fd = pfd[0];
		}
		pids[i] = pid;
	}

	for (i = 0; i < nsegs; i++)
		waitpid(pids[i], 0, 0);
}

int main(void)
{
	char buf[SHELL_BUF_SIZE];
	char cwd[256];

	if (getcwd(cwd, sizeof(cwd)) >= 0)
		setenv("PWD", cwd, 1);

	while (1) {
		shell_prompt();
		shell_readline(buf, SHELL_BUF_SIZE);
		shell_execute(buf);
	}
}
